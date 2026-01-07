#include "network/client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <map>
#include <ctime>
#include <chrono>
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
        std::cerr << "❌ Already connected to server" << std::endl;
        std::cerr << "   Please disconnect first before connecting to a different server" << std::endl;
        return false;
    }
    
    std::cout << "🔌 Connecting to " << host << ":" << port << "..." << std::endl;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "❌ Failed to create network socket" << std::endl;
        std::cerr << "   System error: " << strerror(errno) << std::endl;
        return false;
    }
    
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "❌ Invalid server address: " << host << std::endl;
        std::cerr << "   Please check the address format (should be IPv4, e.g., 127.0.0.1)" << std::endl;
        close(sock);
        return false;
    }
    
    if (::connect(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "❌ Connection failed to " << host << ":" << port << std::endl;
        std::cerr << "   System error: " << strerror(errno) << std::endl;
        std::cerr << "   Possible reasons:" << std::endl;
        std::cerr << "   - Server is not running" << std::endl;
        std::cerr << "   - Firewall is blocking the connection" << std::endl;
        std::cerr << "   - Invalid port number" << std::endl;
        close(sock);
        return false;
    }
    
    connection_ = std::make_unique<Connection>(sock);
    connected_ = true;
    
    std::cout << "✅ Connected successfully to " << host << ":" << port << std::endl;
    std::cout << "   Connection established at " << formatTimestamp(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()) << std::endl;
    
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
        std::cerr << "   Please connect to server first using the 'connect' command" << std::endl;
        return false;
    }
    
    std::cout << "📤 Uploading file from " << localPath << " to " << remotePath << "..." << std::endl;
    
    std::ifstream file(localPath, std::ios::binary);
    if (!file) {
        std::cerr << "❌ Failed to open local file: " << localPath << std::endl;
        std::cerr << "   Please check that the file exists and you have read permissions" << std::endl;
        return false;
    }
    
    // 获取文件大小并显示进度
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<char> data((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    file.close();
    
    std::cout << "📄 File size: " << (fileSize / 1024.0) << " KB" << std::endl;
    
    Protocol::Message request = Protocol::createFileUploadRequest(remotePath, data);
    
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send upload request to server" << std::endl;
        std::cerr << "   Please check your network connection" << std::endl;
        return false;
    }
    
    std::cout << "⏳ Waiting for server response..." << std::endl;
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive upload response from server" << std::endl;
        std::cerr << "   Please check your network connection" << std::endl;
        return false;
    }
    
    if (response.header.type != Protocol::MSG_FILE_UPLOAD_RESPONSE) {
        std::cerr << "❌ Invalid response from server" << std::endl;
        std::cerr << "   Expected file upload response but received type: " 
                  << static_cast<int>(response.header.type) << std::endl;
        return false;
    }
    
    bool success = !response.payload.empty() && response.payload[0] != 0;
    
    if (success) {
        std::cout << "✅ File uploaded successfully to " << remotePath << std::endl;
        std::cout << "   Upload completed at " << formatTimestamp(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()) << std::endl;
    } else {
        std::cerr << "❌ Server failed to process file upload" << std::endl;
        std::cerr << "   Please check server logs for more details" << std::endl;
    }
    
    return success;
}

bool Client::downloadFile(const std::string& remotePath, const std::string& localPath) {
    if (!connected_) {
        std::cerr << "❌ Not connected to server" << std::endl;
        std::cerr << "   Please connect to server first using the 'connect' command" << std::endl;
        return false;
    }
    
    std::cout << "📥 Downloading file from " << remotePath << " to " << localPath << "..." << std::endl;
    
    Protocol::Message request = Protocol::createFileDownloadRequest(remotePath);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send download request to server" << std::endl;
        std::cerr << "   Please check your network connection" << std::endl;
        return false;
    }
    
    std::cout << "⏳ Waiting for server response..." << std::endl;
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive download response from server" << std::endl;
        std::cerr << "   Please check your network connection" << std::endl;
        return false;
    }
    
    bool success;
    std::vector<char> data;
    
    if (!Protocol::parseFileDownloadResponse(response, success, data)) {
        std::cerr << "❌ Failed to parse download response" << std::endl;
        std::cerr << "   Server may be using a different protocol version" << std::endl;
        return false;
    }
    
    if (!success) {
        std::cerr << "❌ Server failed to read file: " << remotePath << std::endl;
        std::cerr << "   Possible reasons:" << std::endl;
        std::cerr << "   - File doesn't exist on server" << std::endl;
        std::cerr << "   - You don't have permission to access the file" << std::endl;
        std::cerr << "   - Server file system error" << std::endl;
        return false;
    }
    
    std::cout << "✅ File received from server, size: " << (data.size() / 1024.0) << " KB" << std::endl;
    
    // 确保本地目录存在
    size_t lastSlashPos = localPath.find_last_of("/");
    if (lastSlashPos != std::string::npos) {
        std::string dirPath = localPath.substr(0, lastSlashPos);
        if (!dirPath.empty()) {
            // 尝试创建目录 (不检查是否成功，因为这可能因权限而失败)
            system(("mkdir -p " + dirPath).c_str());
        }
    }
    
    std::ofstream file(localPath, std::ios::binary);
    if (!file) {
        std::cerr << "❌ Failed to create local file: " << localPath << std::endl;
        std::cerr << "   Please check:" << std::endl;
        std::cerr << "   - You have write permissions in the target directory" << std::endl;
        std::cerr << "   - Disk space is available" << std::endl;
        std::cerr << "   - File path is valid" << std::endl;
        return false;
    }
    
    file.write(data.data(), data.size());
    file.close();
    
    if (!file.good()) {
        std::cerr << "❌ Error occurred while writing to local file" << std::endl;
        std::cerr << "   The downloaded file may be incomplete or corrupted" << std::endl;
        return false;
    }
    
    std::cout << "✅ File downloaded successfully to " << localPath << std::endl;
    std::cout << "   Download completed at " << formatTimestamp(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()) << std::endl;
    
    return true;
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

bool Client::submitPaper(const std::string& title, const std::string& abstract, const std::vector<char>& content, const std::vector<std::string>& keywords) {
    if (!connected_ || sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createSubmitPaperRequest(sessionId_, title, abstract, content, keywords);
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
    
    // 修正：必须包含 sessionId
    request.payload.resize(8);
    uint32_t n_sessionId = htonl(sessionId_);
    uint32_t n_paperId = htonl(paperId);
    
    memcpy(request.payload.data(), &n_sessionId, 4);
    memcpy(request.payload.data() + 4, &n_paperId, 4);
    
    request.header.length = 8;
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
    // 注意：Server::handleGetMyPapers 已经填充了 reviews，所以这里不需要再次调用 getReviewsForPaper
    // 如果 Server 端没有填充，则需要取消注释下面的代码
    /*
    for (auto& paper : result) {
        auto reviews = getReviewsForPaper(paper.paperId);
        paper.reviews = reviews;
    }
    */
    
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
    
    // 🔥 新增：为每篇论文获取评审详情
    // 注意：Server::handleGetAllPapers 已经填充了 reviews (如果实现了的话)
    // 如果 Server 端没有填充，则需要取消注释下面的代码
    /*
    for (auto& paper : result) {
        auto reviews = getReviewsForPaper(paper.paperId);
        paper.reviews = reviews;
    }
    */
    
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
    
    // 🔥 新增：为每篇论文获取评审详情
    // 注意：Server::handleGetPapersToReview 已经填充了 reviews
    /*
    for (auto& paper : result) {
        auto reviews = getReviewsForPaper(paper.paperId);
        paper.reviews = reviews;
    }
    */
    
    return result;
}

// 🔥 新增：获取审稿历史
std::vector<ReviewInfo> Client::getReviewerHistory() {
    std::vector<ReviewInfo> result;
    if (!connected_ || sessionId_ == 0) return result;
    
    Protocol::Message request = Protocol::createGetReviewerHistoryRequest(sessionId_);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send history request" << std::endl;
        return result;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive history response" << std::endl;
        return result;
    }
    
    bool success;
    if (!Protocol::parseGetReviewerHistoryResponse(response, success, result)) {
        std::cerr << "❌ Failed to parse history response" << std::endl;
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

bool Client::autoAssignReviewers(uint32_t paperId) {
    if (!connected_ || sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createAutoAssignRequest(sessionId_, paperId);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send auto assign request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive auto assign response" << std::endl;
        return false;
    }
    
    bool success;
    std::string message;
    if (!Protocol::parseAutoAssignResponse(response, success, message)) {
        std::cerr << "❌ Failed to parse auto assign response" << std::endl;
        return false;
    }
    
    if (success) {
        std::cout << "✅ " << message << std::endl;
    } else {
        std::cerr << "❌ " << message << std::endl;
    }
    
    return success;
}

bool Client::updateProfile(const std::string& institution, const std::vector<std::string>& interests, int maxLoad) {
    if (!connected_ || sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createUpdateProfileRequest(sessionId_, institution, interests, maxLoad);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send update profile request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive update profile response" << std::endl;
        return false;
    }
    
    bool success;
    std::string message;
    if (!Protocol::parseUpdateProfileResponse(response, success, message)) {
        std::cerr << "❌ Failed to parse update profile response" << std::endl;
        return false;
    }
    
    if (success) {
        std::cout << "✅ " << message << std::endl;
    } else {
        std::cerr << "❌ " << message << std::endl;
    }
    
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

// ============================================================================
// 系统监控功能
// ============================================================================

Client::SystemStats Client::getSystemStats() {
    SystemStats stats = {0, 0, 0, 0, 0, 0, 0};
    
    if (!connected_) {
        std::cerr << "❌ Not connected to server" << std::endl;
        std::cerr << "   Please connect to server first using the 'connect' command" << std::endl;
        return stats;
    }
    
    if (sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        std::cerr << "   Please login first using the 'login' command" << std::endl;
        return stats;
    }
    
    std::cout << "📊 Fetching system statistics..." << std::endl;
    
    Protocol::Message request = Protocol::createGetSystemStatsRequest(sessionId_);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send system stats request" << std::endl;
        std::cerr << "   Please check your network connection" << std::endl;
        return stats;
    }
    
    std::cout << "⏳ Waiting for server response..." << std::endl;
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive system stats response" << std::endl;
        std::cerr << "   Please check your network connection" << std::endl;
        return stats;
    }
    
    // 解析系统统计信息：1 字节 success + 32 字节数据
    if (response.payload.size() >= 33) {
        const char* success_ptr = response.payload.data();
        if (success_ptr[0] != 0) { // success
            const char* data = response.payload.data() + 1;
            
            // 解析 uint64_t uptime
            uint64_t uptime;
            std::memcpy(&uptime, data, 8);
            stats.uptime_seconds = uptime;
            
            // 解析后续的 uint32_t 值
            const uint32_t* uint32Data = reinterpret_cast<const uint32_t*>(data + 8);
            stats.total_connections = uint32Data[0];
            stats.active_connections = uint32Data[1];
            stats.total_requests = uint32Data[2];
            stats.cache_hit_rate = uint32Data[3];
            stats.memory_usage_mb = uint32Data[4];
            stats.disk_usage_mb = uint32Data[5];
        } else {
            std::cerr << "❌ Server returned error when fetching system statistics" << std::endl;
            std::cerr << "   Please check server logs for more details" << std::endl;
        }
    } else {
        std::cerr << "❌ Invalid response format from server" << std::endl;
        std::cerr << "   Server may be using a different protocol version" << std::endl;
    }
    
    return stats;
}

std::vector<std::pair<uint32_t, std::string>> Client::getOnlineUsers() {
    std::vector<std::pair<uint32_t, std::string>> users;
    
    if (!connected_) {
        std::cerr << "❌ Not connected to server" << std::endl;
        std::cerr << "   Please connect to server first using the 'connect' command" << std::endl;
        return users;
    }
    
    if (sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        std::cerr << "   Please login first using the 'login' command" << std::endl;
        return users;
    }
    
    std::cout << "👥 Fetching online users list..." << std::endl;
    
    Protocol::Message request = Protocol::createListOnlineUsersRequest(sessionId_);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send online users request" << std::endl;
        std::cerr << "   Please check your network connection" << std::endl;
        return users;
    }
    
    std::cout << "⏳ Waiting for server response..." << std::endl;
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive online users response" << std::endl;
        std::cerr << "   Please check your network connection" << std::endl;
        return users;
    }
    
    // 解析在线用户列表：1 字节 success + 4 字节 count + 变长数据
    if (response.payload.size() >= 5) {
        const char* success_ptr = response.payload.data();
        if (success_ptr[0] != 0) { // success
            const uint32_t* countPtr = reinterpret_cast<const uint32_t*>(response.payload.data() + 1);
            uint32_t count = countPtr[0];
            
            if (count == 0) {
                std::cout << "ℹ️ No users are currently online" << std::endl;
                return users;
            }
            
            const char* data = response.payload.data() + 5;
            size_t offset = 0;
            
            for (uint32_t i = 0; i < count; i++) {
                if (offset + 8 > response.payload.size() - 5) break;
                
                // 解析 sessionId
                uint32_t sessionId;
                std::memcpy(&sessionId, data + offset, 4);
                offset += 4;
                
                // 解析用户名长度
                uint32_t nameLength;
                std::memcpy(&nameLength, data + offset, 4);
                offset += 4;
                
                // 解析用户名
                if (offset + nameLength <= response.payload.size() - 5) {
                    std::string username(data + offset, nameLength);
                    offset += nameLength;
                    users.emplace_back(sessionId, username);
                }
            }
            
            std::cout << "✅ Found " << users.size() << " online user(s)" << std::endl;
        } else {
            std::cerr << "❌ Server returned error when fetching online users" << std::endl;
            std::cerr << "   Please check server logs for more details" << std::endl;
        }
    } else {
        std::cerr << "❌ Invalid response format from server" << std::endl;
        std::cerr << "   Server may be using a different protocol version" << std::endl;
    }
    
    return users;
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
// ============================================================================
// 🔥 新增功能实现
// ============================================================================

// 🔥 Update Paper File
bool Client::updatePaperFile(uint32_t paperId, const std::vector<char>& content) {
    if (!connected_ || sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createUpdatePaperFileRequest(sessionId_, paperId, content);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send update paper request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive update paper response" << std::endl;
        return false;
    }
    
    bool success;
    std::string message;
    if (!Protocol::parseUpdatePaperFileResponse(response, success, message)) {
        std::cerr << "❌ Failed to parse update response" << std::endl;
        return false;
    }
    
    if (success) {
        std::cout << "✅ " << message << std::endl;
    } else {
        std::cerr << "❌ Update failed: " << message << std::endl;
    }
    
    return success;
}

bool Client::uploadRevision(uint32_t paperId, const std::vector<char>& content) {
    if (!connected_ || sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createUploadRevisionRequest(sessionId_, paperId, content);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send upload revision request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive upload revision response" << std::endl;
        return false;
    }
    
    bool success;
    std::string message;
    if (!Protocol::parseUploadRevisionResponse(response, success, message)) {
        std::cerr << "❌ Failed to parse upload revision response" << std::endl;
        return false;
    }
    
    if (success) {
        std::cout << "✅ " << message << std::endl;
    } else {
        std::cerr << "❌ " << message << std::endl;
    }
    
    return success;
}

bool Client::downloadPaper(uint32_t paperId, const std::string& localPath) {
    if (!connected_ || sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createDownloadPaperRequest(sessionId_, paperId);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send download paper request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive download paper response" << std::endl;
        return false;
    }
    
    bool success;
    std::vector<char> data;
    if (!Protocol::parseDownloadPaperResponse(response, success, data)) {
        std::cerr << "❌ Failed to parse download paper response" << std::endl;
        return false;
    }
    
    if (success) {
        std::ofstream file(localPath, std::ios::binary);
        if (!file) {
            std::cerr << "❌ Failed to create local file: " << localPath << std::endl;
            return false;
        }
        file.write(data.data(), data.size());
        std::cout << "✅ Paper downloaded successfully to " << localPath << " (" << data.size() << " bytes)" << std::endl;
    } else {
        std::cerr << "❌ Failed to download paper (File not found or permission denied)" << std::endl;
    }
    
    return success;
}

bool Client::makeDecision(uint32_t paperId, const std::string& decision) {
    if (!connected_ || sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createMakeDecisionRequest(sessionId_, paperId, decision);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send make decision request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive make decision response" << std::endl;
        return false;
    }
    
    bool success;
    std::string message;
    if (!Protocol::parseMakeDecisionResponse(response, success, message)) {
        std::cerr << "❌ Failed to parse make decision response" << std::endl;
        return false;
    }
    
    if (success) {
        std::cout << "✅ " << message << std::endl;
    } else {
        std::cerr << "❌ " << message << std::endl;
    }
    
    return success;
}

bool Client::updateUserRole(uint32_t userId, const std::string& role) {
    if (!connected_ || sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createUpdateUserRoleRequest(sessionId_, userId, role);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send update user role request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive update user role response" << std::endl;
        return false;
    }
    
    bool success;
    std::string message;
    if (!Protocol::parseUpdateUserRoleResponse(response, success, message)) {
        std::cerr << "❌ Failed to parse update user role response" << std::endl;
        return false;
    }
    
    if (success) {
        std::cout << "✅ " << message << std::endl;
    } else {
        std::cerr << "❌ " << message << std::endl;
    }
    
    return success;
}

bool Client::deactivateUser(uint32_t userId) {
    if (!connected_ || sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createDeactivateUserRequest(sessionId_, userId);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send deactivate user request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive deactivate user response" << std::endl;
        return false;
    }
    
    bool success;
    std::string message;
    if (!Protocol::parseDeactivateUserResponse(response, success, message)) {
        std::cerr << "❌ Failed to parse deactivate user response" << std::endl;
        return false;
    }
    
    if (success) {
        std::cout << "✅ " << message << std::endl;
    } else {
        std::cerr << "❌ " << message << std::endl;
    }
    
    return success;
}

bool Client::systemBackup() {
    if (!connected_ || sessionId_ == 0) {
        std::cerr << "❌ Not logged in" << std::endl;
        return false;
    }
    
    Protocol::Message request = Protocol::createSystemBackupRequest(sessionId_);
    if (!connection_->sendMessage(request)) {
        std::cerr << "❌ Failed to send system backup request" << std::endl;
        return false;
    }
    
    Protocol::Message response;
    if (!connection_->receiveMessage(response)) {
        std::cerr << "❌ Failed to receive system backup response" << std::endl;
        return false;
    }
    
    bool success;
    std::string message;
    if (!Protocol::parseSystemBackupResponse(response, success, message)) {
        std::cerr << "❌ Failed to parse system backup response" << std::endl;
        return false;
    }
    
    if (success) {
        std::cout << "✅ " << message << std::endl;
    } else {
        std::cerr << "❌ " << message << std::endl;
    }
    
    return success;
}
