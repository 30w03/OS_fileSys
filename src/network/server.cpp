#include "network/server.h"
#include "protocol/protocol.h"
#include "user/user.h"
#include "review/paper.h"
#include "review/review.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <sstream>

Server::Server(uint16_t port, const std::string& diskImage)
    : port_(port), diskImage_(diskImage), serverSocket_(-1), running_(false) {
    
    // 创建文件系统
    filesystem_ = std::make_shared<Filesystem>(diskImage);
    
    // 创建用户管理器
    userManager_ = std::make_shared<UserManager>();
    
    // 创建审稿系统
    reviewSystem_ = std::make_shared<ReviewSystem>(filesystem_, userManager_);
}

Server::~Server() {
    stop();
}

bool Server::start() {
    // 1. 挂载文件系统
    std::cout << "Mounting filesystem from: " << diskImage_ << std::endl;
    
    if (!filesystem_->mount()) {
        std::cerr << "❌ Failed to mount filesystem" << std::endl;
        return false;
    }
    
    std::cout << "✅ Filesystem mounted successfully" << std::endl;
    
    // 2. 加载用户数据
    std::cout << "Loading user data..." << std::endl;

    
    // 创建默认管理员账户（如果不存在）
    User admin;
    if (!userManager_->getUserById(1, admin)) {
        if (userManager_->createUser("admin", "admin123", UserRole::ADMIN)) {
            std::cout << "✅ Default admin account created (username: admin, password: admin123)" << std::endl;
        }
    }
    
    // 3. 创建 socket
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
    
    // 4. 设置 socket 选项（允许地址重用）
    int opt = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options" << std::endl;
        close(serverSocket_);
        return false;
    }
    
    // 5. 绑定地址
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);
    
    if (bind(serverSocket_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind socket to port " << port_ << std::endl;
        close(serverSocket_);
        return false;
    }
    
    // 6. 开始监听
    if (listen(serverSocket_, 10) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        close(serverSocket_);
        return false;
    }
    
    running_ = true;
    
    std::cout << "✅ Server started successfully on port " << port_ << std::endl;
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Peer Review System Server Ready" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    return true;
}

void Server::stop() {
    if (running_) {
        running_ = false;
        
        std::cout << "\nShutting down server..." << std::endl;
        
        // 保存用户数据
        userManager_->saveToFile("users.dat");
        
        // 关闭服务器 socket
        if (serverSocket_ >= 0) {
            close(serverSocket_);
            serverSocket_ = -1;
        }
        
        // 等待接受线程结束
        if (acceptThread_.joinable()) {
            acceptThread_.join();
        }
        
        // 等待所有客户端线程结束
        for (auto& thread : clientThreads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        clientThreads_.clear();
        
        // 卸载文件系统
        if (filesystem_) {
            filesystem_->unmount();
        }
        
        std::cout << "✅ Server stopped" << std::endl;
    }
}

void Server::run() {
    acceptLoop();
}

void Server::acceptLoop() {
    while (running_) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket_, 
                                  reinterpret_cast<sockaddr*>(&clientAddr), 
                                  &clientLen);
        
        if (clientSocket < 0) {
            if (running_) {
                std::cerr << "Failed to accept client connection" << std::endl;
            }
            continue;
        }
        
        auto connection = std::make_unique<Connection>(clientSocket);
        
        std::cout << "📥 New client connected from " 
                  << connection->getRemoteAddress() << ":" 
                  << connection->getRemotePort() << std::endl;
        
        clientThreads_.emplace_back(&Server::handleClient, this, std::move(connection));
    }
}

void Server::handleClient(std::unique_ptr<Connection> client) {
    std::cout << "🔧 Handling client from " << client->getRemoteAddress() 
              << ":" << client->getRemotePort() << std::endl;
    
    Protocol::Message request, response;
    
    while (client->isConnected() && running_) {
        // 接收请求
        if (!client->receiveMessage(request)) {
            break;
        }
        
        std::cout << "📨 Received message type: " << static_cast<int>(request.header.type) << std::endl;
        
        // 处理不同类型的请求
        switch (request.header.type) {
            case Protocol::MSG_PING:
                std::cout << "  → PING" << std::endl;
                response = Protocol::createPongMessage();
                break;
                
            case Protocol::MSG_LOGIN_REQUEST:
                std::cout << "  → LOGIN_REQUEST" << std::endl;
                handleLogin(client.get(), request);
                continue;
                
            case Protocol::MSG_REGISTER_REQUEST:
                std::cout << "  → REGISTER_REQUEST" << std::endl;
                handleRegister(client.get(), request);
                continue;
                
            case Protocol::MSG_LOGOUT_REQUEST:
                std::cout << "  → LOGOUT_REQUEST" << std::endl;
                handleLogout(client.get(), request);
                continue;
                
            case Protocol::MSG_FILE_LIST_REQUEST: {
                std::cout << "  → FILE_LIST_REQUEST" << std::endl;
                auto files = filesystem_->listFiles();
                std::cout << "  Found " << files.size() << " files" << std::endl;
                response = Protocol::createFileListResponse(files);
                break;
            }
                
            case Protocol::MSG_FILE_UPLOAD_REQUEST: {
                std::string path;
                std::vector<char> data;
                
                if (Protocol::parseFileUploadRequest(request, path, data)) {
                    std::cout << "  → FILE_UPLOAD_REQUEST: " << path 
                             << " (" << data.size() << " bytes)" << std::endl;
                    
                    bool success = filesystem_->writeFile(path, data);
                    response = Protocol::createFileUploadResponse(success);
                    
                    if (success) {
                        std::cout << "  ✅ File uploaded successfully" << std::endl;
                    } else {
                        std::cerr << "  ❌ Failed to upload file" << std::endl;
                    }
                } else {
                    std::cerr << "  ❌ Failed to parse upload request" << std::endl;
                    response = Protocol::createFileUploadResponse(false);
                }
                break;
            }
                
            case Protocol::MSG_FILE_DOWNLOAD_REQUEST: {
                std::string path;
                
                if (Protocol::parseFileDownloadRequest(request, path)) {
                    std::cout << "  → FILE_DOWNLOAD_REQUEST: " << path << std::endl;
                    
                    std::vector<char> data;
                    bool success = filesystem_->readFile(path, data);
                    response = Protocol::createFileDownloadResponse(success, data);
                    
                    if (success) {
                        std::cout << "  ✅ File downloaded: " << data.size() << " bytes" << std::endl;
                    } else {
                        std::cerr << "  ❌ Failed to download file" << std::endl;
                    }
                } else {
                    std::cerr << "  ❌ Failed to parse download request" << std::endl;
                    response = Protocol::createFileDownloadResponse(false, {});
                }
                break;
            }
                
            case Protocol::MSG_FILE_DELETE_REQUEST: {
                std::string path;
                
                if (Protocol::parseFileDeleteRequest(request, path)) {
                    std::cout << "  → FILE_DELETE_REQUEST: " << path << std::endl;
                    
                    bool success = filesystem_->deleteFile(path);
                    response = Protocol::createFileDeleteResponse(success);
                } else {
                    response = Protocol::createFileDeleteResponse(false);
                }
                break;
            }
            
            // ========== 审稿系统消息 ==========
            case Protocol::MSG_SUBMIT_PAPER_REQUEST:
                std::cout << "  → SUBMIT_PAPER_REQUEST" << std::endl;
                handleSubmitPaper(client.get(), request);
                continue;
                
            case Protocol::MSG_GET_MY_PAPERS_REQUEST:
                std::cout << "  → GET_MY_PAPERS_REQUEST" << std::endl;
                handleGetMyPapers(client.get(), request);
                continue;
                
            case Protocol::MSG_GET_PAPERS_TO_REVIEW_REQUEST:
                std::cout << "  → GET_PAPERS_TO_REVIEW_REQUEST" << std::endl;
                handleGetPapersToReview(client.get(), request);
                continue;
                
            case Protocol::MSG_SUBMIT_REVIEW_REQUEST:
                std::cout << "  → SUBMIT_REVIEW_REQUEST" << std::endl;
                handleSubmitReview(client.get(), request);
                continue;
                
            case Protocol::MSG_ASSIGN_REVIEWER_REQUEST:
                std::cout << "  → ASSIGN_REVIEWER_REQUEST" << std::endl;
                handleAssignReviewer(client.get(), request);
                continue;
                
            case Protocol::MSG_GET_ALL_PAPERS_REQUEST:
                std::cout << "  → GET_ALL_PAPERS_REQUEST" << std::endl;
                handleGetAllPapers(client.get(), request);
                continue;
                
            case Protocol::MSG_DOWNLOAD_PAPER_REQUEST:
                std::cout << "  → DOWNLOAD_PAPER_REQUEST" << std::endl;
                handleDownloadPaper(client.get(), request);
                continue;
                
            case Protocol::MSG_GET_REVIEWS_REQUEST:
                std::cout << "  → GET_REVIEWS_REQUEST" << std::endl;
                handleGetReviews(client.get(), request);
                continue;
                
            case Protocol::MSG_MAKE_DECISION_REQUEST:
                std::cout << "  → MAKE_DECISION_REQUEST" << std::endl;
                handleMakeDecision(client.get(), request);
                continue;
                
            case Protocol::MSG_GET_STATISTICS_REQUEST:
                std::cout << "  → GET_STATISTICS_REQUEST" << std::endl;
                handleGetStatistics(client.get(), request);
                continue;
                
            default:
                std::cerr << "  ❌ Unknown message type: " << static_cast<int>(request.header.type) << std::endl;
                continue;
        }
        
        // 发送响应
        if (!client->sendMessage(response)) {
            std::cerr << "  ❌ Failed to send response" << std::endl;
            break;
        }
    }
    
    std::cout << "📤 Client disconnected from " << client->getRemoteAddress() 
              << ":" << client->getRemotePort() << std::endl;
}

// ============================================================================
// 用户认证处理
// ============================================================================
void Server::handleLogin(Connection* client, const Protocol::Message& request) {
    std::string username, password;
    
    if (!Protocol::parseLoginRequest(request, username, password)) {
        auto response = Protocol::createLoginResponse(false, 0, 0, "");
        client->sendMessage(response);
        return;
    }
    
    uint32_t userId;
    bool success = userManager_->authenticateUser(username, password, userId);
    
    Protocol::Message response;
    if (success) {
        uint32_t sessionId = userManager_->createSession(userId);
        
        User user;
        userManager_->getUserById(userId, user);
        
        std::cout << "  ✅ Login successful: " << username 
                  << " (ID: " << userId << ", Role: " << user.getRoleName() << ")" << std::endl;
        
        response = Protocol::createLoginResponse(true, sessionId, userId, user.getRoleName());
    } else {
        std::cout << "  ❌ Login failed: " << username << std::endl;
        response = Protocol::createLoginResponse(false, 0, 0, "");
    }
    
    client->sendMessage(response);
}

void Server::handleRegister(Connection* client, const Protocol::Message& request) {
    std::string username, password, roleStr;
    
    if (!Protocol::parseRegisterRequest(request, username, password, roleStr)) {
        auto response = Protocol::createRegisterResponse(false, "Invalid request");
        client->sendMessage(response);
        return;
    }
    
    // 解析角色
    UserRole role = UserRole::AUTHOR;
    if (roleStr == "REVIEWER") role = UserRole::REVIEWER;
    else if (roleStr == "EDITOR") role = UserRole::EDITOR;
    else if (roleStr == "ADMIN") role = UserRole::ADMIN;
    
    bool success = userManager_->createUser(username, password, role);
    
    Protocol::Message response;
    if (success) {
        std::cout << "  ✅ User registered: " << username << " as " << roleStr << std::endl;
        response = Protocol::createRegisterResponse(true, "Registration successful");
    } else {
        std::cout << "  ❌ Registration failed: " << username << std::endl;
        response = Protocol::createRegisterResponse(false, "Username already exists");
    }
    
    client->sendMessage(response);
}

void Server::handleLogout(Connection* client, const Protocol::Message& request) {
    // TODO: 解析 sessionId 并使其失效
    auto response = Protocol::createLogoutResponse(true);
    client->sendMessage(response);
}

// ============================================================================
// 论文提交处理
// ============================================================================
void Server::handleSubmitPaper(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId;
    std::string title, abstract;
    std::vector<char> fileData;
    
    if (!Protocol::parseSubmitPaperRequest(request, sessionId, title, abstract, fileData)) {
        auto response = Protocol::createSubmitPaperResponse(false, 0, "Invalid request");
        client->sendMessage(response);
        return;
    }
    
    // 验证会话
    uint32_t userId;
    if (!userManager_->validateSession(sessionId, userId)) {
        auto response = Protocol::createSubmitPaperResponse(false, 0, "Invalid session");
        client->sendMessage(response);
        return;
    }
    
    // 提交论文
    uint32_t paperId = reviewSystem_->submitPaper(userId, title, abstract, fileData);
    
    Protocol::Message response;
    if (paperId > 0) {
        response = Protocol::createSubmitPaperResponse(true, paperId, "Paper submitted successfully");
    } else {
        response = Protocol::createSubmitPaperResponse(false, 0, "Failed to submit paper");
    }
    
    client->sendMessage(response);
}

// ============================================================================
// 获取我的论文
// ============================================================================
void Server::handleGetMyPapers(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId;
    
    if (!Protocol::parseGetMyPapersRequest(request, sessionId)) {
        auto response = Protocol::createGetMyPapersResponse(false, {});
        client->sendMessage(response);
        return;
    }
    
    uint32_t userId;
    if (!userManager_->validateSession(sessionId, userId)) {
        auto response = Protocol::createGetMyPapersResponse(false, {});
        client->sendMessage(response);
        return;
    }
    
    auto papers = reviewSystem_->getPapersByAuthor(userId);
    
    // 转换为 PaperInfo 结构
    std::vector<PaperInfo> paperInfos;
    for (const auto& paper : papers) {
        PaperInfo info;
        info.paperId = paper.paperId;
        info.title = paper.title;
        info.abstract = paper.abstract;
        info.submissionTime = paper.submissionTime;
        info.currentVersion = paper.currentVersion;
        info.authorIds = paper.authorIds;
        info.reviewerIds = paper.assignedReviewers;
        
        // 🔥 获取该论文的所有评审
        auto reviews = reviewSystem_->getReviewsForPaper(paper.paperId);
        
        // 🔥 根据评审情况智能显示状态
        if (reviews.empty()) {
            // 没有评审
            if (paper.assignedReviewers.empty()) {
                info.status = "Submitted";
            } else {
                info.status = "Under Review";
            }
        } else if (reviews.size() < paper.assignedReviewers.size()) {
            // 部分审稿人已提交
            info.status = "Under Review (" + std::to_string(reviews.size()) + "/" + 
                         std::to_string(paper.assignedReviewers.size()) + " reviews)";
        } else {
            // 所有审稿人都已提交，统计决定
            int acceptCount = 0;
            int rejectCount = 0;
            
            for (const auto& rev : reviews) {
                if (rev.decision == ReviewDecision::STRONG_ACCEPT || 
                    rev.decision == ReviewDecision::ACCEPT ||
                    rev.decision == ReviewDecision::WEAK_ACCEPT) {
                    acceptCount++;
                } else if (rev.decision == ReviewDecision::STRONG_REJECT || 
                           rev.decision == ReviewDecision::REJECT ||
                           rev.decision == ReviewDecision::WEAK_REJECT) {
                    rejectCount++;
                }
            }
            
            // 🔥 智能显示综合决定
            if (paper.status == PaperStatus::ACCEPTED) {
                info.status = "✅ Accepted";
            } else if (paper.status == PaperStatus::REJECTED) {
                info.status = "❌ Rejected";
            } else if (rejectCount > acceptCount) {
                info.status = "Reviewed - Likely Reject (" + std::to_string(rejectCount) + " reject, " + 
                             std::to_string(acceptCount) + " accept)";
            } else if (acceptCount > rejectCount) {
                info.status = "Reviewed - Likely Accept (" + std::to_string(acceptCount) + " accept, " + 
                             std::to_string(rejectCount) + " reject)";
            } else {
                info.status = "Reviewed - Mixed Decision";
            }
        }
        
        // 🔥 填充评审详情
        for (const auto& review : reviews) {
            ReviewInfo reviewInfo;
            reviewInfo.reviewId = review.reviewId;
            reviewInfo.paperId = review.paperId;
            reviewInfo.reviewerId = review.reviewerId;
            reviewInfo.decision = review.getDecisionName();
            reviewInfo.confidenceScore = review.confidenceScore;
            reviewInfo.comments = review.comments;
            reviewInfo.submitTime = static_cast<uint64_t>(review.submitTime);
            
            // 获取审稿人用户名
            User reviewer;
            if (userManager_->getUserById(review.reviewerId, reviewer)) {
                reviewInfo.reviewerName = reviewer.username;
            } else {
                reviewInfo.reviewerName = "User" + std::to_string(review.reviewerId);
            }
            
            info.reviews.push_back(reviewInfo);
        }
        
        paperInfos.push_back(info);
    }
    
    auto response = Protocol::createGetMyPapersResponse(true, paperInfos);
    client->sendMessage(response);
}

// ============================================================================
// 获取待审论文
// ============================================================================
void Server::handleGetPapersToReview(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId;
    
    if (!Protocol::parseGetPapersToReviewRequest(request, sessionId)) {
        auto response = Protocol::createGetPapersToReviewResponse(false, {});
        client->sendMessage(response);
        return;
    }
    
    uint32_t userId;
    if (!userManager_->validateSession(sessionId, userId)) {
        auto response = Protocol::createGetPapersToReviewResponse(false, {});
        client->sendMessage(response);
        return;
    }
    
    auto papers = reviewSystem_->getPapersToReview(userId);
    
    std::vector<PaperInfo> paperInfos;
    for (const auto& paper : papers) {
        PaperInfo info;
        info.paperId = paper.paperId;
        info.title = paper.title;
        info.abstract = paper.abstract;
        info.status = paper.getStatusName();
        info.submissionTime = paper.submissionTime;
        info.currentVersion = paper.currentVersion;
        info.authorIds = paper.authorIds;
        info.reviewerIds = paper.assignedReviewers;
        paperInfos.push_back(info);
    }
    
    auto response = Protocol::createGetPapersToReviewResponse(true, paperInfos);
    client->sendMessage(response);
}


// ============================================================================
// 提交评审
// ============================================================================
void Server::handleSubmitReview(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId, paperId;
    std::string decision, comments;
    int confidence;
    
    if (!Protocol::parseSubmitReviewRequest(request, sessionId, paperId, decision, confidence, comments)) {
        auto response = Protocol::createSubmitReviewResponse(false, 0, "Invalid request");
        client->sendMessage(response);
        return;
    }
    
    uint32_t userId;
    if (!userManager_->validateSession(sessionId, userId)) {
        auto response = Protocol::createSubmitReviewResponse(false, 0, "Invalid session");
        client->sendMessage(response);
        return;
    }
    
    // 解析决定
    ReviewDecision dec = ReviewDecision::BORDERLINE;
    if (decision == "STRONG_ACCEPT") dec = ReviewDecision::STRONG_ACCEPT;
    else if (decision == "ACCEPT") dec = ReviewDecision::ACCEPT;
    else if (decision == "WEAK_ACCEPT") dec = ReviewDecision::WEAK_ACCEPT;
    else if (decision == "WEAK_REJECT") dec = ReviewDecision::WEAK_REJECT;
    else if (decision == "REJECT") dec = ReviewDecision::REJECT;
    else if (decision == "STRONG_REJECT") dec = ReviewDecision::STRONG_REJECT;
    
    uint32_t reviewId = reviewSystem_->submitReview(userId, paperId, dec, confidence, comments);
    
    Protocol::Message response;
    if (reviewId > 0) {
        response = Protocol::createSubmitReviewResponse(true, reviewId, "Review submitted successfully");
    } else {
        response = Protocol::createSubmitReviewResponse(false, 0, "Failed to submit review");
    }
    
    client->sendMessage(response);
}

// ============================================================================
// 分配审稿人
// ============================================================================
void Server::handleAssignReviewer(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId, paperId, reviewerId;
    
    if (!Protocol::parseAssignReviewerRequest(request, sessionId, paperId, reviewerId)) {
        auto response = Protocol::createAssignReviewerResponse(false, "Invalid request");
        client->sendMessage(response);
        return;
    }
    
    uint32_t userId;
    if (!userManager_->validateSession(sessionId, userId)) {
        auto response = Protocol::createAssignReviewerResponse(false, "Invalid session");
        client->sendMessage(response);
        return;
    }
    
    bool success = reviewSystem_->assignReviewer(userId, paperId, reviewerId);
    
    Protocol::Message response;
    if (success) {
        response = Protocol::createAssignReviewerResponse(true, "Reviewer assigned successfully");
    } else {
        response = Protocol::createAssignReviewerResponse(false, "Failed to assign reviewer");
    }
    
    client->sendMessage(response);
}

// ============================================================================
// 获取所有论文（编辑）
// ============================================================================
void Server::handleGetAllPapers(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId;
    
    if (!Protocol::parseGetAllPapersRequest(request, sessionId)) {
        auto response = Protocol::createGetAllPapersResponse(false, {});
        client->sendMessage(response);
        return;
    }
    
    uint32_t userId;
    if (!userManager_->validateSession(sessionId, userId)) {
        auto response = Protocol::createGetAllPapersResponse(false, {});
        client->sendMessage(response);
        return;
    }
    
    auto papers = reviewSystem_->getAllPapers();
    
    std::vector<PaperInfo> paperInfos;
    for (const auto& paper : papers) {
        PaperInfo info;
        info.paperId = paper.paperId;
        info.title = paper.title;
        info.abstract = paper.abstract;
        info.status = paper.getStatusName();
        info.submissionTime = paper.submissionTime;
        info.currentVersion = paper.currentVersion;
        info.authorIds = paper.authorIds;
        info.reviewerIds = paper.assignedReviewers;
        paperInfos.push_back(info);
    }
    
    auto response = Protocol::createGetAllPapersResponse(true, paperInfos);
    client->sendMessage(response);
}

// ============================================================================
// 下载论文
// ============================================================================
void Server::handleDownloadPaper(Connection* client, const Protocol::Message& request) {
    // TODO: 实现完整的下载逻辑（需要解析请求）
    auto response = Protocol::createDownloadPaperResponse(false, {});
    client->sendMessage(response);
}

// ============================================================================
// 获取评审
// ============================================================================
void Server::handleGetReviews(Connection* client, const Protocol::Message& request) {
    if (request.payload.size() < 4) {
        auto response = Protocol::createGetReviewsResponse(false, {});
        client->sendMessage(response);
        return;
    }
    
    const uint32_t* ptr = reinterpret_cast<const uint32_t*>(request.payload.data());
    uint32_t paperId = ptr[0];
    
    auto reviews = reviewSystem_->getReviewsForPaper(paperId);
    
    std::vector<ReviewInfo> reviewInfos;
    for (const auto& review : reviews) {
        ReviewInfo info;
        info.reviewId = review.reviewId;
        info.paperId = review.paperId;
        info.reviewerId = review.reviewerId;
        info.decision = review.getDecisionName();
        info.confidenceScore = review.confidenceScore;
        info.comments = review.comments;
        info.submitTime = static_cast<uint64_t>(review.submitTime);  // ✅ 修复
        
        User reviewer;
        if (userManager_->getUserById(review.reviewerId, reviewer)) {
            info.reviewerName = reviewer.username;  // ✅ 修复
        } else {
            info.reviewerName = "User" + std::to_string(review.reviewerId);
        }
        
        reviewInfos.push_back(info);
    }
    
    auto response = Protocol::createGetReviewsResponse(true, reviewInfos);
    client->sendMessage(response);
}

// ============================================================================
// 编辑决定
// ============================================================================
void Server::handleMakeDecision(Connection* client, const Protocol::Message& request) {
    // TODO: 实现
    auto response = Protocol::createMakeDecisionResponse(false, "Not implemented");
    client->sendMessage(response);
}

// ============================================================================
// 统计信息
// ============================================================================
void Server::handleGetStatistics(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId;
    
    // 解析请求
    if (request.payload.size() >= 4) {
        const uint32_t* ptr = reinterpret_cast<const uint32_t*>(request.payload.data());
        sessionId = ptr[0];
    }
    
    // 获取统计信息
    auto paperStats = reviewSystem_->getStatistics();
    
    // 计算总数
    uint32_t total_papers = 0;
    uint32_t pending_papers = 0;
    uint32_t accepted_papers = 0;
    uint32_t rejected_papers = 0;
    
    for (const auto& [status, count] : paperStats) {
        total_papers += count;
        
        if (status == PaperStatus::SUBMITTED || 
            status == PaperStatus::UNDER_REVIEW || 
            status == PaperStatus::REVIEWED) {
            pending_papers += count;
        } else if (status == PaperStatus::ACCEPTED) {
            accepted_papers += count;
        } else if (status == PaperStatus::REJECTED) {
            rejected_papers += count;
        }
    }
    
    // ✅ 修复：计算总评审数
    uint32_t total_reviews = 0;
    for (uint32_t pid = 1; pid <= total_papers; pid++) {
        auto reviews = reviewSystem_->getReviewsForPaper(pid);
        total_reviews += reviews.size();
    }
    
    // 构造二进制响应
    Protocol::Message response;
    response.header.type = Protocol::MSG_GET_STATISTICS_RESPONSE;
    
    response.payload.push_back(1);  // success = true
    
    auto writeUint32 = [](std::vector<char>& payload, uint32_t value) {
        payload.insert(payload.end(), 
                      reinterpret_cast<char*>(&value), 
                      reinterpret_cast<char*>(&value) + 4);
    };
    
    writeUint32(response.payload, total_papers);
    writeUint32(response.payload, total_reviews);  // ✅ 修复：使用实际计算的值
    writeUint32(response.payload, userManager_->getUserCount());
    writeUint32(response.payload, pending_papers);
    writeUint32(response.payload, accepted_papers);
    writeUint32(response.payload, rejected_papers);
    
    response.header.payloadSize = response.payload.size();
    response.header.checksum = Protocol::calculateChecksum(response.payload);
    
    client->sendMessage(response);
}