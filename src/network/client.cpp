#include "network/client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <map>
#include <ctime>
#include <iomanip>
#include <sstream>

// ===== 时间格式化辅助函数 =====
std::string formatTimestamp(uint64_t timestamp) {
    if (timestamp == 0) {
        return "N/A";
    }
    
    time_t time = static_cast<time_t>(timestamp);
    std::tm* tm = std::localtime(&time);
    
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
    return std::string(buffer);
}

Client::Client() : connected_(false), sessionId_(0) {}

Client::~Client() {
    disconnect();
}

bool Client::connect(const std::string& host, uint16_t port) {
    if (connected_) {
        std::cerr << "Already connected" << std::endl;
        return false;
    }
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
    
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address: " << host << std::endl;
        close(sock);
        return false;
    }
    
    if (::connect(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed to " << host << ":" << port << std::endl;
        close(sock);
        return false;
    }
    
    connection_ = std::make_unique<Connection>(sock);
    connected_ = true;
    
    std::cout << "Connected to " << host << ":" << port << std::endl;
    
    return true;
}

void Client::disconnect() {
    if (connected_) {
        connection_.reset();
        connected_ = false;
        sessionId_ = 0;
        std::cout << "Disconnected from server" << std::endl;
    }
}

bool Client::isConnected() const {
    return connected_;
}

bool Client::ping() {
    if (!connected_) {
        return false;
    }
    
    Protocol::Message request = Protocol::createPingMessage();
    
    if (!connection_->sendMessage(request)) {
        std::cerr << "Failed to send ping" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "Failed to receive pong" << std::endl;
        return false;
    }
    
    return response.header.type == Protocol::MSG_PONG;
}

std::vector<FileListEntry> Client::listFiles() {
    std::vector<FileListEntry> result;
    
    if (!connected_) {
        return result;
    }
    
    Protocol::Message request = Protocol::createFileListRequest();
    if (!connection_->sendMessage(request)) {
        std::cerr << "Failed to send file list request" << std::endl;
        return result;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "Failed to receive file list response" << std::endl;
        return result;
    }
    
    if (!Protocol::parseFileListResponse(response, result)) {
        std::cerr << "Failed to parse file list response" << std::endl;
        result.clear();
    }
    
    return result;
}

bool Client::uploadFile(const std::string& localPath, const std::string& remotePath) {
    if (!connected_) {
        std::cerr << "❌ Not connected to server" << std::endl;
        return false;
    }
    
    std::ifstream file(localPath, std::ios::binary);
    if (!file) {
        std::cerr << "❌ Failed to open local file: " << localPath << std::endl;
        return false;
    }
    
    std::vector<char> data((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    file.close();
    
    Protocol::Message request = Protocol::createFileUploadRequest(remotePath, data);
    
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send upload request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive upload response" << std::endl;
        return false;
    }
    
    if (response.header.type != Protocol::MSG_FILE_UPLOAD_RESPONSE) {
        std::cerr << "❌ Invalid response type" << std::endl;
        return false;
    }
    
    bool success = !response.payload.empty() && response.payload[0] != 0;
    return success;
}

bool Client::downloadFile(const std::string& remotePath, const std::string& localPath) {
    if (!connected_) {
        return false;
    }
    
    Protocol::Message request = Protocol::createFileDownloadRequest(remotePath);
    if (!connection_->sendMessage(request)) {
        std::cerr << "Failed to send download request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "Failed to receive download response" << std::endl;
        return false;
    }
    
    bool success;
    std::vector<char> data;
    
    if (!Protocol::parseFileDownloadResponse(response, success, data)) {
        std::cerr << "Failed to parse download response" << std::endl;
        return false;
    }
    
    if (!success) {
        std::cerr << "Server failed to read file: " << remotePath << std::endl;
        return false;
    }
    
    std::ofstream file(localPath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open local file for writing: " << localPath << std::endl;
        return false;
    }
    
    file.write(data.data(), data.size());
    file.close();
    
    return file.good();
}

bool Client::deleteFile(const std::string& remotePath) {
    if (!connected_) {
        return false;
    }
    
    Protocol::Message request = Protocol::createFileDeleteRequest(remotePath);
    if (!connection_->sendMessage(request)) {
        std::cerr << "Failed to send delete request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "Failed to receive delete response" << std::endl;
        return false;
    }
    
    if (response.header.type != Protocol::MSG_FILE_DELETE_RESPONSE) {
        std::cerr << "Invalid response type" << std::endl;
        return false;
    }
    
    bool success = !response.payload.empty() && response.payload[0] != 0;
    return success;
}

// ===== 用户管理方法 =====

bool Client::registerUser(const std::string& username, const std::string& password, const std::string& role) {
    if (!connected_) {
        std::cerr << "❌ Not connected" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createRegisterRequest(username, password, role);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send register request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive register response" << std::endl;
        return false;
    }
    
    bool success;
    std::string message;
    if (!Protocol::parseRegisterResponse(response, success, message)) {
        std::cerr << "❌ Failed to parse register response" << std::endl;
        return false;
    }
    
    if (!success && !message.empty()) {
        std::cerr << "❌ " << message << std::endl;
    }
    
    return success;
}

bool Client::login(const std::string& username, const std::string& password) {
    if (!connected_) {
        std::cerr << "❌ Not connected" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createLoginRequest(username, password);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send login request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive login response" << std::endl;
        return false;
    }
    
    bool success;
    uint32_t userId;
    std::string role;
    
    if (!Protocol::parseLoginResponse(response, success, sessionId_, userId, role)) {
        std::cerr << "❌ Failed to parse login response" << std::endl;
        return false;
    }
    
    if (success) {
        std::cout << "✅ Logged in as " << role << " (User ID: " << userId 
                  << ", Session ID: " << sessionId_ << ")" << std::endl;
    }
    
    return success;
}

bool Client::logout() {
    if (!connected_ || sessionId_ == 0) {
        return false;
    }
    
    Protocol::Message request = Protocol::createLogoutRequest(sessionId_);
    if (!connection_->sendMessage(request)) {
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        return false;
    }
    
    if (response.header.type == Protocol::MSG_LOGOUT_RESPONSE) {
        sessionId_ = 0;
        return true;
    }
    
    return false;
}

// ===== 论文管理方法 =====

bool Client::submitPaper(const std::string& title, const std::string& abstract, const std::vector<char>& content) {
    if (!connected_ || sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createSubmitPaperRequest(sessionId_, title, abstract, content);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send submit paper request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive submit paper response" << std::endl;
        return false;
    }
    
    bool success;
    uint32_t paperId;
    std::string message;
    
    if (!Protocol::parseSubmitPaperResponse(response, success, paperId, message)) {
        std::cerr << "❌ Failed to parse submit paper response" << std::endl;
        return false;
    }
    
    if (success) {
        std::cout << "✅ Paper submitted with ID: " << paperId << std::endl;
    } else if (!message.empty()) {
        std::cerr << "❌ " << message << std::endl;
    }
    
    return success;
}

// 🔥 新增辅助函数：获取指定论文的评审详情
std::vector<ReviewInfo> Client::getReviewsForPaper(uint32_t paperId) {
    std::vector<ReviewInfo> result;
    
    if (!connected_) {
        return result;
    }
    
    // 构造请求
    Protocol::Message request;
    request.header.type = Protocol::MSG_GET_REVIEWS_REQUEST;
    request.payload.resize(4);
    memcpy(request.payload.data(), &paperId, 4);
    request.header.payloadSize = 4;
    request.header.checksum = Protocol::calculateChecksum(request.payload);
    
    if (!connection_->sendMessage(request)) {
        return result;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        return result;
    }
    
    // 解析响应
    if (response.header.type == Protocol::MSG_GET_REVIEWS_RESPONSE && response.payload.size() > 0) {
        bool success = response.payload[0] != 0;
        if (success) {
            Protocol::parseGetReviewsResponse(response, success, result);
        }
    }
    
    return result;
}

std::vector<PaperInfo> Client::getMyPapers() {
    std::vector<PaperInfo> result;
    
    if (!connected_ || sessionId_ == 0) {
        return result;
    }
    
    Protocol::Message request = Protocol::createGetMyPapersRequest(sessionId_);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send get my papers request" << std::endl;
        return result;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive get my papers response" << std::endl;
        return result;
    }
    
    bool success;
    if (!Protocol::parseGetMyPapersResponse(response, success, result)) {
        std::cerr << "❌ Failed to parse papers response" << std::endl;
        result.clear();
        return result;
    }
    
    // 🔥 新增：为每篇论文获取评审详情
    for (auto& paper : result) {
        auto reviews = getReviewsForPaper(paper.paperId);
        paper.reviews = reviews;
    }
    
    return result;
}

std::vector<PaperInfo> Client::getAllPapers() {
    std::vector<PaperInfo> result;
    
    if (!connected_ || sessionId_ == 0) {
        return result;
    }
    
    Protocol::Message request = Protocol::createGetAllPapersRequest(sessionId_);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send get all papers request" << std::endl;
        return result;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive get all papers response" << std::endl;
        return result;
    }
    
    bool success;
    if (!Protocol::parseGetAllPapersResponse(response, success, result)) {
        std::cerr << "❌ Failed to parse papers response" << std::endl;
        result.clear();
    }
    
    return result;
}

// ===== 评审管理方法 =====

std::vector<PaperInfo> Client::getPapersToReview() {
    std::vector<PaperInfo> result;
    
    if (!connected_ || sessionId_ == 0) {
        return result;
    }
    
    Protocol::Message request = Protocol::createGetPapersToReviewRequest(sessionId_);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send get papers to review request" << std::endl;
        return result;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive get papers to review response" << std::endl;
        return result;
    }
    
    bool success;
    if (!Protocol::parseGetPapersToReviewResponse(response, success, result)) {
        std::cerr << "❌ Failed to parse papers response" << std::endl;
        result.clear();
    }
    
    return result;
}

bool Client::submitReview(uint32_t paperId, const std::string& decision, int score, const std::string& comment) {
    if (!connected_ || sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createSubmitReviewRequest(sessionId_, paperId, decision, score, comment);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send submit review request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive submit review response" << std::endl;
        return false;
    }
    
    bool success;
    uint32_t reviewId;
    std::string message;
    
    if (!Protocol::parseSubmitReviewResponse(response, success, reviewId, message)) {
        std::cerr << "❌ Failed to parse submit review response" << std::endl;
        return false;
    }
    
    if (success) {
        std::cout << "✅ Review submitted with ID: " << reviewId << std::endl;
    } else if (!message.empty()) {
        std::cerr << "❌ " << message << std::endl;
    }
    
    return success;
}

// ===== 编辑操作方法 =====

bool Client::assignReviewer(uint32_t paperId, uint32_t reviewerId) {
    if (!connected_ || sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createAssignReviewerRequest(sessionId_, paperId, reviewerId);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send assign reviewer request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive assign reviewer response" << std::endl;
        return false;
    }
    
    if (response.header.type != Protocol::MSG_ASSIGN_REVIEWER_RESPONSE) {
        std::cerr << "❌ Invalid response type" << std::endl;
        return false;
    }
    
    bool success = !response.payload.empty() && response.payload[0] != 0;
    return success;
}

Statistics Client::getStatistics() {
    Statistics stats = {0, 0, 0, 0, 0, 0};
    
    if (!connected_ || sessionId_ == 0) {
        return stats;
    }
    
    Protocol::Message request = Protocol::createGetStatisticsRequest(sessionId_);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send statistics request" << std::endl;
        return stats;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive statistics response" << std::endl;
        return stats;
    }
    
    // 解析统计信息：1 字节 success + 24 字节数据
    if (response.payload.size() >= 25) {
        const uint32_t* data = reinterpret_cast<const uint32_t*>(response.payload.data() + 1);
        stats.total_papers = data[0];
        stats.total_reviews = data[1];
        stats.total_users = data[2];
        stats.pending_papers = data[3];
        stats.accepted_papers = data[4];
        stats.rejected_papers = data[5];
    }
    
    return stats;
}

// ===== 🔥 显示我的论文（带评审详情）=====
void Client::viewMyPapers() {
    auto papers = getMyPapers();
    
    if (papers.empty()) {
        std::cout << "\n📭 You have no submitted papers.\n" << std::endl;
        return;
    }
    
    std::cout << "\n📚 Your Papers (" << papers.size() << "):\n" << std::endl;
    
    for (const auto& paper : papers) {
        std::cout << "┌─────────────────────────────────────────" << std::endl;
        std::cout << "│ Paper ID: " << paper.paperId << std::endl;
        std::cout << "│ Title: " << paper.title << std::endl;
        std::cout << "│ Abstract: " << paper.abstract << std::endl;
        std::cout << "│ Status: " << paper.status << std::endl;
        std::cout << "│ Submitted: " << formatTimestamp(paper.submissionTime) << std::endl;
        std::cout << "│ Version: " << paper.currentVersion << std::endl;
        std::cout << "│ Authors: " << paper.authorIds.size() << std::endl;
        std::cout << "│ Reviewers: " << paper.reviewerIds.size() << std::endl;
        
        // 🔥 显示评审详情
        if (!paper.reviews.empty()) {
            std::cout << "│" << std::endl;
            std::cout << "│ 📝 Reviews (" << paper.reviews.size() << "):" << std::endl;
            
            for (size_t i = 0; i < paper.reviews.size(); i++) {
                const auto& review = paper.reviews[i];
                
                std::cout << "│   ┌─ Review #" << (i + 1) << " ─────────────────" << std::endl;
                std::cout << "│   │ Reviewer: " << review.reviewerName 
                         << " (ID: " << review.reviewerId << ")" << std::endl;
                std::cout << "│   │ Decision: " << review.decision << std::endl;
                std::cout << "│   │ Confidence: " << review.confidenceScore << "/5" << std::endl;
                std::cout << "│   │ Comments: " << review.comments << std::endl;
                std::cout << "│   │ Submitted: " << formatTimestamp(review.submitTime) << std::endl;
                std::cout << "│   └─────────────────────────────────" << std::endl;
            }
        } else {
            std::cout << "│" << std::endl;
            std::cout << "│ ⏳ No reviews submitted yet" << std::endl;
        }
        
        std::cout << "└─────────────────────────────────────────\n" << std::endl;
    }
}