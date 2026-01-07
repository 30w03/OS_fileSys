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
#include <chrono>
#include <sys/time.h>
#include <sys/resource.h>
#include <iomanip> // Added for setw, setfill
#include <algorithm> // Added for transform

Server::Server(uint16_t port, const std::string& diskImage)
    : port_(port), diskImage_(diskImage), serverSocket_(-1), running_(false),
      startTime_(std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch()).count()),
      totalConnections_(0), totalRequests_(0) {
    
    // 创建文件系统
    filesystem_ = std::make_shared<Filesystem>(diskImage);
    
    // 创建用户管理器
    userManager_ = std::make_shared<UserManager>();
    
    // 创建审稿系统
    reviewSystem_ = std::make_shared<ReviewSystem>(filesystem_, userManager_);
    
    // 创建HTTP处理器
    httpHandler_ = std::make_shared<HttpHandler>(userManager_, reviewSystem_, filesystem_);
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
    
    // Ensure system directories exist
    auto dirOps = filesystem_->getDirOps();
    if (!filesystem_->exists("/papers")) {
        std::cout << "Creating /papers directory..." << std::endl;
        dirOps->mkdir("/papers");
    }
    if (!filesystem_->exists("/reviews")) {
        std::cout << "Creating /reviews directory..." << std::endl;
        dirOps->mkdir("/reviews");
    }
    if (!filesystem_->exists("/system")) {
        std::cout << "Creating /system directory..." << std::endl;
        dirOps->mkdir("/system");
    }

    // 2. 加载用户数据
    std::cout << "Loading user data..." << std::endl;
    userManager_->loadFromFile("users.dat");
    
    // 3. 初始化评审系统 (加载元数据)
    std::cout << "Initializing review system..." << std::endl;
    reviewSystem_->init();

    // 创建默认管理员账户（如果不存在）
    User admin;
    if (!userManager_->getUserById(1, admin)) {
        if (userManager_->createUser("admin", "admin123", UserRole::ADMIN)) {
            std::cout << "✅ Default admin account created (username: admin, password: admin123)" << std::endl;
        }
    }
    
    // 3. 创建 socket (Force IPv4 for better compatibility)
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
    
    // 增加连接计数
    totalConnections_++;
    
    // 首先检测请求类型
    if (isHttpRequest(client.get())) {
        std::cout << "🌐 Detected HTTP request" << std::endl;
        handleHttpRequest(std::move(client));
        return;
    }
    
    // 如果不是HTTP请求，则处理自定义协议
    Protocol::Message request, response;
    
    while (client->isConnected() && running_) {
        // 接收请求
        if (!client->receiveMessage(request)) {
            break;
        }
        
        // 增加请求计数
        totalRequests_++;
        
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
                    // Ensure path starts with /
                    if (!path.empty() && path[0] != '/') {
                        path = "/" + path;
                    }

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
                    // Ensure path starts with /
                    if (!path.empty() && path[0] != '/') {
                        path = "/" + path;
                    }

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
                    // Ensure path starts with /
                    if (!path.empty() && path[0] != '/') {
                        path = "/" + path;
                    }

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

            case Protocol::MSG_AUTO_ASSIGN_REQUEST:
                std::cout << "  → AUTO_ASSIGN_REQUEST" << std::endl;
                handleAutoAssign(client.get(), request);
                continue;

            case Protocol::MSG_UPDATE_PROFILE_REQUEST:
                std::cout << "  → UPDATE_PROFILE_REQUEST" << std::endl;
                handleUpdateProfile(client.get(), request);
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

            case Protocol::MSG_GET_REVIEWER_HISTORY_REQUEST: // 🔥 New
                std::cout << "  → GET_REVIEWER_HISTORY_REQUEST" << std::endl;
                handleGetReviewerHistory(client.get(), request);
                continue;
                
            case Protocol::MSG_MAKE_DECISION_REQUEST:
                std::cout << "  → MAKE_DECISION_REQUEST" << std::endl;
                handleMakeDecision(client.get(), request);
                continue;
                
            case Protocol::MSG_GET_STATISTICS_REQUEST:
                std::cout << "  → GET_STATISTICS_REQUEST" << std::endl;
                handleGetStatistics(client.get(), request);
                continue;
                
            case Protocol::MSG_GET_SYSTEM_STATS_REQUEST:
                std::cout << "  → GET_SYSTEM_STATS_REQUEST" << std::endl;
                handleGetSystemStats(client.get(), request);
                continue;
                
            case Protocol::MSG_LIST_ONLINE_USERS_REQUEST:
                std::cout << "  → LIST_ONLINE_USERS_REQUEST" << std::endl;
                handleListOnlineUsers(client.get(), request);
                continue;
                
            case Protocol::MSG_UPLOAD_REVISION_REQUEST:
                std::cout << "  → UPLOAD_REVISION_REQUEST" << std::endl;
                handleUploadRevision(client.get(), request);
                continue;
                
            case Protocol::MSG_UPDATE_PAPER_FILE_REQUEST:
                std::cout << "  → UPDATE_PAPER_FILE_REQUEST" << std::endl;
                handleUpdatePaperFile(client.get(), request);
                continue;

            case Protocol::MSG_UPDATE_USER_ROLE_REQUEST:
                std::cout << "  → UPDATE_USER_ROLE_REQUEST" << std::endl;
                handleUpdateUserRole(client.get(), request);
                continue;
                
            case Protocol::MSG_DEACTIVATE_USER_REQUEST:
                std::cout << "  → DEACTIVATE_USER_REQUEST" << std::endl;
                handleDeactivateUser(client.get(), request);
                continue;
                
            case Protocol::MSG_SYSTEM_BACKUP_REQUEST:
                std::cout << "  → SYSTEM_BACKUP_REQUEST" << std::endl;
                handleSystemBackup(client.get(), request);
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

// ============================================================================
// 系统监控功能
// ============================================================================

void Server::handleGetSystemStats(Connection* client, const Protocol::Message& request) {
    // 获取系统运行时间
    uint64_t currentTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    uint64_t uptime = currentTime - startTime_.load();
    
    // 获取活跃连接数
    uint32_t activeConnections = clientThreads_.size();
    
    // 获取内存使用情况
    struct rusage usage;
    uint32_t memoryUsageMB = 0;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        memoryUsageMB = usage.ru_maxrss / 1024; // KB to MB
    }
    
    // 获取磁盘使用情况
    uint32_t diskUsageMB = 0;
    // 使用文件系统 API 获取实际使用空间
    if (filesystem_) {
        diskUsageMB = filesystem_->getUsedSpace() / (1024 * 1024);
    } else {
        // Fallback: Check file size if filesystem not ready
        std::ifstream diskFile(diskImage_, std::ios::binary | std::ios::ate);
        if (diskFile.is_open()) {
            diskUsageMB = diskFile.tellg() / (1024 * 1024);
            diskFile.close();
        }
    }
    
    // 构造响应
    Protocol::Message response;
    response.header.type = Protocol::MSG_GET_SYSTEM_STATS_RESPONSE;
    response.payload.push_back(1); // success = true
    
    auto writeUint64 = [](std::vector<char>& payload, uint64_t value) {
        payload.insert(payload.end(), 
                      reinterpret_cast<char*>(&value), 
                      reinterpret_cast<char*>(&value) + 8);
    };
    
    auto writeUint32 = [](std::vector<char>& payload, uint32_t value) {
        payload.insert(payload.end(), 
                      reinterpret_cast<char*>(&value), 
                      reinterpret_cast<char*>(&value) + 4);
    };
    
    writeUint64(response.payload, uptime);
    writeUint32(response.payload, totalConnections_.load());
    writeUint32(response.payload, activeConnections);
    writeUint32(response.payload, totalRequests_.load());
    writeUint32(response.payload, 85); // 模拟缓存命中率
    writeUint32(response.payload, memoryUsageMB);
    writeUint32(response.payload, diskUsageMB);
    
    response.header.length = response.payload.size();
    response.header.checksum = Protocol::calculateChecksum(response.payload);
    
    client->sendMessage(response);
}

// ============================================================================
// HTTP请求检测和处理
// ============================================================================

bool Server::isHttpRequest(Connection* client) {
    // 检查前几个字节是否为HTTP请求
    char buffer[8];
    if (!client->peekFirstBytes(buffer, sizeof(buffer))) {
        return false;
    }
    
    // HTTP请求以 "GET ", "POST ", "PUT ", "DELETE " 等开始
    // 检查是否为文本字符并且包含HTTP关键词
    std::string firstBytes(buffer, buffer + sizeof(buffer));
    
    // 检查是否包含HTTP方法的特征
    if (firstBytes.rfind("GET ", 0) == 0 ||
        firstBytes.rfind("POST ", 0) == 0 ||
        firstBytes.rfind("PUT ", 0) == 0 ||
        firstBytes.rfind("DELETE ", 0) == 0 ||
        firstBytes.rfind("HEAD ", 0) == 0 ||
        firstBytes.rfind("OPTIONS ", 0) == 0 ||
        firstBytes.rfind("PATCH ", 0) == 0) {
        return true;
    }
    
    // 自定义协议以 PRSF (0x50525346) 开头
    uint32_t magic;
    std::memcpy(&magic, buffer, 4);
    // Convert from network byte order to host byte order
    magic = ntohl(magic);
    
    if (magic == 0x50525346) { // "PRSF"
        return false;
    }
    
    // 如果既不是HTTP也不是自定义协议，默认当作HTTP处理（为了兼容性）
    return true;
}

void Server::handleHttpRequest(std::unique_ptr<Connection> client) {
    // 读取完整的HTTP请求
    std::string requestData;
    char buffer[4096];
    
    while (client->isConnected()) {
        ssize_t received = recv(client->getSocket(), buffer, sizeof(buffer), 0);
        
        if (received <= 0) {
            break;
        }
        
        requestData.append(buffer, received);
        
        // 检查是否收到了完整的HTTP headers
        if (requestData.find("\r\n\r\n") != std::string::npos) {
            break;
        }
        
        // 防止无限循环
        if (requestData.size() > 65536) { // 64KB
            break;
        }
    }

    // 检查 Content-Length 并读取剩余 Body
    size_t headerEnd = requestData.find("\r\n\r\n");
    if (headerEnd != std::string::npos) {
        size_t contentLength = 0;
        
        // 简单的查找 Content-Length
        // 为了兼容性，查找几种常见的大小写形式
        const char* clKeys[] = {"Content-Length:", "content-length:", "CONTENT-LENGTH:"};
        for (const char* key : clKeys) {
            size_t keyLen = strlen(key);
            size_t clPos = requestData.find(key);
            if (clPos != std::string::npos && clPos < headerEnd) {
                size_t valStart = clPos + keyLen;
                size_t valEnd = requestData.find("\r\n", valStart);
                if (valEnd != std::string::npos) {
                    std::string valStr = requestData.substr(valStart, valEnd - valStart);
                    // trim spaces
                    size_t first = valStr.find_first_not_of(" \t");
                    if (first != std::string::npos) {
                        valStr.erase(0, first);
                    }
                    try {
                        contentLength = std::stoul(valStr);
                    } catch(...) {
                        contentLength = 0;
                    }
                    break; 
                }
            }
        }

        if (contentLength > 0) {
            size_t currentBodyLen = requestData.size() - (headerEnd + 4);
            size_t needed = (contentLength > currentBodyLen) ? (contentLength - currentBodyLen) : 0;
            
            // 如果还需要读取更多数据
            while (needed > 0 && client->isConnected()) {
                ssize_t received = recv(client->getSocket(), buffer, sizeof(buffer), 0);
                
                if (received <= 0) {
                    break;
                }
                
                requestData.append(buffer, received);
                
                if (static_cast<size_t>(received) >= needed) {
                    needed = 0;
                } else {
                    needed -= received;
                }
                
                // 安全限制：防止 Body 过大导致内存耗尽 (例如限制 50MB)
                if (requestData.size() > 50 * 1024 * 1024) {
                    std::cerr << "Request too large, aborting" << std::endl;
                    break;
                }
            }
        }
    }
    
    if (requestData.empty()) {
        return;
    }
    
    // 解析HTTP请求
    std::istringstream requestStream(requestData);
    std::string method, path, httpVersion;
    requestStream >> method >> path >> httpVersion;
    
    // 读取headers（简单解析）
    std::map<std::string, std::string> headers;
    std::string line;
    std::getline(requestStream, line); // 跳过第一行的剩余部分
    
    while (std::getline(requestStream, line) && !line.empty()) {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            // 去除空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            headers[key] = value;
        }
    }
    
    // 读取body
    std::string body;
    size_t bodyStart = requestData.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        body = requestData.substr(bodyStart + 4);
    }
    
    std::cout << "🌐 HTTP Request: " << method << " " << path << std::endl;
    
    // 增加HTTP请求计数
    totalRequests_++;
    
    // 使用HTTP处理器处理请求
    std::string httpResponse = httpHandler_->handleRequest(method, path, body, headers);
    
    // 发送响应
    client->sendHttpResponse(httpResponse);
}

void Server::handleListOnlineUsers(Connection* client, const Protocol::Message& request) {
    // 构造响应
    Protocol::Message response;
    response.header.type = Protocol::MSG_LIST_ONLINE_USERS_RESPONSE;
    response.payload.push_back(1); // success = true
    
    // 获取在线用户列表
    std::lock_guard<std::mutex> lock(onlineUsersMutex_);
    uint32_t onlineCount = onlineUsers_.size();
    
    auto writeUint32 = [](std::vector<char>& payload, uint32_t value) {
        payload.insert(payload.end(), 
                      reinterpret_cast<char*>(&value), 
                      reinterpret_cast<char*>(&value) + 4);
    };
    
    writeUint32(response.payload, onlineCount);
    
    // 添加每个在线用户的信息
    for (const auto& [sessionId, username] : onlineUsers_) {
        writeUint32(response.payload, sessionId);
        
        // 添加用户名长度和内容
        uint32_t nameLength = username.length();
        writeUint32(response.payload, nameLength);
        response.payload.insert(response.payload.end(), username.begin(), username.end());
    }
    
    response.header.length = response.payload.size();
    response.header.checksum = Protocol::calculateChecksum(response.payload);
    
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
    std::string roleUpper = roleStr;
    std::transform(roleUpper.begin(), roleUpper.end(), roleUpper.begin(), ::toupper);
    
    if (roleUpper == "REVIEWER") role = UserRole::REVIEWER;
    else if (roleUpper == "EDITOR") role = UserRole::EDITOR;
    else if (roleUpper == "ADMIN") role = UserRole::ADMIN;
    
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
    std::vector<std::string> keywords;
    
    if (!Protocol::parseSubmitPaperRequest(request, sessionId, title, abstract, fileData, keywords)) {
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
    uint32_t paperId = reviewSystem_->submitPaper(userId, title, abstract, fileData, keywords);
    
    Protocol::Message response;
    if (paperId > 0) {
        response = Protocol::createSubmitPaperResponse(true, paperId, "Paper submitted successfully");
    } else {
        response = Protocol::createSubmitPaperResponse(false, 0, "Failed to submit paper");
    }
    
    client->sendMessage(response);
}

// ============================================================================
// 自动分配处理
// ============================================================================
void Server::handleAutoAssign(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId;
    uint32_t paperId;
    
    if (!Protocol::parseAutoAssignRequest(request, sessionId, paperId)) {
        auto response = Protocol::createAutoAssignResponse(false, "Invalid request");
        client->sendMessage(response);
        return;
    }
    
    uint32_t userId;
    if (!userManager_->validateSession(sessionId, userId)) {
        auto response = Protocol::createAutoAssignResponse(false, "Invalid session");
        client->sendMessage(response);
        return;
    }
    
    // 检查权限 (Admin or Editor)
    User user;
    if (!userManager_->getUserById(userId, user) || (user.role != UserRole::ADMIN && user.role != UserRole::EDITOR)) {
        auto response = Protocol::createAutoAssignResponse(false, "Permission denied");
        client->sendMessage(response);
        return;
    }
    
    bool success = reviewSystem_->autoAssignReviewers(paperId);
    
    Protocol::Message response;
    if (success) {
        response = Protocol::createAutoAssignResponse(true, "Auto assignment completed successfully");
    } else {
        response = Protocol::createAutoAssignResponse(false, "Failed to auto assign reviewers (maybe no candidates found)");
    }
    
    client->sendMessage(response);
}

// ============================================================================
// 更新资料处理
// ============================================================================
void Server::handleUpdateProfile(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId;
    std::string institution;
    std::vector<std::string> interests;
    int maxLoad;
    
    if (!Protocol::parseUpdateProfileRequest(request, sessionId, institution, interests, maxLoad)) {
        auto response = Protocol::createUpdateProfileResponse(false, "Invalid request");
        client->sendMessage(response);
        return;
    }
    
    uint32_t userId;
    if (!userManager_->validateSession(sessionId, userId)) {
        auto response = Protocol::createUpdateProfileResponse(false, "Invalid session");
        client->sendMessage(response);
        return;
    }
    
    bool success = userManager_->updateUserProfile(userId, institution, interests, maxLoad);
    
    Protocol::Message response;
    if (success) {
        response = Protocol::createUpdateProfileResponse(true, "Profile updated successfully");
    } else {
        response = Protocol::createUpdateProfileResponse(false, "Failed to update profile");
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
        
        // 获取该论文的所有评审
        auto reviews = reviewSystem_->getReviewsForPaper(paper.paperId);
        
        // 填充评审详情
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
    
    // 解析决定 (Case-insensitive)
    std::string d = decision;
    std::transform(d.begin(), d.end(), d.begin(), ::toupper);
    
    ReviewDecision dec = ReviewDecision::BORDERLINE;
    if (d == "STRONG_ACCEPT") dec = ReviewDecision::STRONG_ACCEPT;
    else if (d == "ACCEPT") dec = ReviewDecision::ACCEPT;
    else if (d == "WEAK_ACCEPT") dec = ReviewDecision::WEAK_ACCEPT;
    else if (d == "WEAK_REJECT") dec = ReviewDecision::WEAK_REJECT;
    else if (d == "REJECT") dec = ReviewDecision::REJECT;
    else if (d == "STRONG_REJECT") dec = ReviewDecision::STRONG_REJECT;
    
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
        
        // 获取该论文的所有评审
        auto reviews = reviewSystem_->getReviewsForPaper(paper.paperId);
        
        // 填充评审详情
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
    
    auto response = Protocol::createGetAllPapersResponse(true, paperInfos);
    client->sendMessage(response);
}

// ============================================================================
// 下载论文
// ============================================================================
void Server::handleDownloadPaper(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId;
    uint32_t paperId;
    
    // Manual parsing since parseDownloadPaperRequest is missing in header
    if (request.payload.size() < 8) {
        auto response = Protocol::createDownloadPaperResponse(false, {});
        client->sendMessage(response);
        return;
    }
    
    const uint32_t* ptr = reinterpret_cast<const uint32_t*>(request.payload.data());
    // Network byte order to host byte order
    sessionId = ntohl(ptr[0]);
    paperId = ntohl(ptr[1]);
    
    uint32_t userId;
    if (!userManager_->validateSession(sessionId, userId)) {
        auto response = Protocol::createDownloadPaperResponse(false, {});
        client->sendMessage(response);
        return;
    }
    
    // 获取论文信息以找到文件路径
    // ReviewSystem::getPaperInfo returns Paper object by value
    Paper paper = reviewSystem_->getPaperInfo(paperId);
    if (paper.paperId == 0) { // Assuming 0 means not found
        std::cerr << "Paper not found: " << paperId << std::endl;
        auto response = Protocol::createDownloadPaperResponse(false, {});
        client->sendMessage(response);
        return;
    }
    
    // 生成文件路径 (假设 ReviewSystem 存储的是相对路径或我们知道规则)
    // 这里我们假设 ReviewSystem 内部管理路径，但我们需要路径来访问文件系统
    // 由于 ReviewSystem 没有直接暴露 getPaperPath，我们可能需要修改 ReviewSystem
    // 或者假设路径格式。查看 ReviewSystem::submitPaper，路径是 generatePaperPath 生成的
    // 我们可以尝试通过 paperInfo 获取，但 PaperInfo 可能不包含路径
    
    // 让我们先看看 ReviewSystem 是否有 getPaperPath
    // 如果没有，我们可能需要先修改 ReviewSystem
    
    // 假设 PaperInfo 没有路径，我们需要从 ReviewSystem 获取
    // 暂时先用硬编码的规则或修改 ReviewSystem
    // 为了稳妥，我先去修改 ReviewSystem 增加 getPaperPath 接口
    
    // Use the filepath from the Paper object directly
    std::string path = paper.filepath;
    if (path.empty()) {
        // Fallback if filepath is empty for some reason
        std::ostringstream oss;
        oss << "/papers/paper_" << std::setw(6) << std::setfill('0') << paperId;
        if (paper.currentVersion > 1) {
            oss << "_v" << paper.currentVersion;
        }
        oss << ".pdf";
        path = oss.str();
    }
    
    std::cout << "User " << userId << " attempting to download " << path << std::endl;
    
    // Check if user is Editor or Admin to grant access
    User user;
    uint32_t accessUserId = userId;
    if (userManager_->getUserById(userId, user)) {
        if (user.role == UserRole::EDITOR || user.role == UserRole::ADMIN) {
            accessUserId = 1; // Use admin ID for file access
        }
    }

    // 使用 ACL 感知的读取操作
    // 注意：这里我们直接调用 fileOps，绕过 Filesystem 的默认 Admin 权限
    std::vector<char> data;
    size_t size = filesystem_->getFileOps()->getFileSize(accessUserId, path);
    
    if (size == 0) {
        // 可能是权限拒绝，也可能是文件不存在
        // 再次检查是否存在
        if (!filesystem_->getFileOps()->fileExists(accessUserId, path)) {
             std::cerr << "File not found or permission denied (size=0)" << std::endl;
             auto response = Protocol::createDownloadPaperResponse(false, {});
             client->sendMessage(response);
             return;
        }
    }
    
    data.resize(size);
    ssize_t bytesRead = filesystem_->getFileOps()->readFile(accessUserId, path, data.data(), size);
    
    if (bytesRead < 0) {
        std::cerr << "❌ Permission denied or read error for user " << userId << " on " << path << std::endl;
        auto response = Protocol::createDownloadPaperResponse(false, {});
        client->sendMessage(response);
        return;
    }
    
    data.resize(bytesRead);
    auto response = Protocol::createDownloadPaperResponse(true, data);
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
// 获取审稿历史
// ============================================================================
void Server::handleGetReviewerHistory(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId;
    if (!Protocol::parseGetReviewerHistoryRequest(request, sessionId)) {
        auto response = Protocol::createGetReviewerHistoryResponse(false, {});
        client->sendMessage(response);
        return;
    }
    
    uint32_t userId;
    if (!userManager_->validateSession(sessionId, userId)) {
        auto response = Protocol::createGetReviewerHistoryResponse(false, {});
        client->sendMessage(response);
        return;
    }
    
    auto reviews = reviewSystem_->getReviewsByReviewer(userId);
    
    std::vector<ReviewInfo> reviewInfos;
    for (const auto& review : reviews) {
        ReviewInfo info;
        info.reviewId = review.reviewId;
        info.paperId = review.paperId;
        info.reviewerId = review.reviewerId;
        info.decision = review.getDecisionName();
        info.confidenceScore = review.confidenceScore;
        info.comments = review.comments;
        info.submitTime = static_cast<uint64_t>(review.submitTime);
        
        User reviewer;
        if (userManager_->getUserById(review.reviewerId, reviewer)) {
            info.reviewerName = reviewer.username;
        } else {
            info.reviewerName = "User" + std::to_string(review.reviewerId);
        }
        
        reviewInfos.push_back(info);
    }
    
    auto response = Protocol::createGetReviewerHistoryResponse(true, reviewInfos);
    client->sendMessage(response);
}

// ============================================================================
// 编辑决定
// ============================================================================
void Server::handleMakeDecision(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId, paperId;
    std::string decisionStr;
    
    if (!Protocol::parseMakeDecisionRequest(request, sessionId, paperId, decisionStr)) {
        auto response = Protocol::createMakeDecisionResponse(false, "Invalid request");
        client->sendMessage(response);
        return;
    }
    
    uint32_t userId;
    if (!userManager_->validateSession(sessionId, userId)) {
        auto response = Protocol::createMakeDecisionResponse(false, "Invalid session");
        client->sendMessage(response);
        return;
    }
    
    // Case insensitive comparison
    std::transform(decisionStr.begin(), decisionStr.end(), decisionStr.begin(), ::toupper);

    PaperStatus status = PaperStatus::SUBMITTED;
    if (decisionStr == "ACCEPTED" || decisionStr == "ACCEPT") status = PaperStatus::ACCEPTED;
    else if (decisionStr == "REJECTED" || decisionStr == "REJECT") status = PaperStatus::REJECTED;
    else {
        auto response = Protocol::createMakeDecisionResponse(false, "Invalid decision status");
        client->sendMessage(response);
        return;
    }
    
    if (reviewSystem_->makeFinalDecision(userId, paperId, status)) {
        auto response = Protocol::createMakeDecisionResponse(true, "Decision recorded successfully");
        client->sendMessage(response);
    } else {
        auto response = Protocol::createMakeDecisionResponse(false, "Failed to record decision (Permission denied or invalid state)");
        client->sendMessage(response);
    }
}

// ============================================================================
// 上传修订版
// ============================================================================
void Server::handleUploadRevision(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId, paperId;
    std::vector<char> fileData;
    
    if (!Protocol::parseUploadRevisionRequest(request, sessionId, paperId, fileData)) {
        auto response = Protocol::createUploadRevisionResponse(false, "Invalid request");
        client->sendMessage(response);
        return;
    }
    
    uint32_t userId;
    if (!userManager_->validateSession(sessionId, userId)) {
        auto response = Protocol::createUploadRevisionResponse(false, "Invalid session");
        client->sendMessage(response);
        return;
    }
    
    if (reviewSystem_->uploadRevision(paperId, userId, fileData)) {
        auto response = Protocol::createUploadRevisionResponse(true, "Revision uploaded successfully");
        client->sendMessage(response);
    } else {
        auto response = Protocol::createUploadRevisionResponse(false, "Failed to upload revision");
        client->sendMessage(response);
    }
}

// ============================================================================
// 更新论文文件
// ============================================================================
void Server::handleUpdatePaperFile(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId, paperId;
    std::vector<char> fileData;
    
    if (!Protocol::parseUpdatePaperFileRequest(request, sessionId, paperId, fileData)) {
        auto response = Protocol::createUpdatePaperFileResponse(false, "Invalid request");
        client->sendMessage(response);
        return;
    }
    
    uint32_t userId;
    if (!userManager_->validateSession(sessionId, userId)) {
        auto response = Protocol::createUpdatePaperFileResponse(false, "Invalid session");
        client->sendMessage(response);
        return;
    }
    
    if (reviewSystem_->updatePaperFile(paperId, userId, fileData)) {
        auto response = Protocol::createUpdatePaperFileResponse(true, "Paper updated successfully");
        client->sendMessage(response);
    } else {
        auto response = Protocol::createUpdatePaperFileResponse(false, "Failed to update paper (Check ownership or status)");
        client->sendMessage(response);
    }
}

// ============================================================================
// 更新用户角色
// ============================================================================
void Server::handleUpdateUserRole(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId, targetUserId;
    std::string roleStr;
    
    if (!Protocol::parseUpdateUserRoleRequest(request, sessionId, targetUserId, roleStr)) {
        auto response = Protocol::createUpdateUserRoleResponse(false, "Invalid request");
        client->sendMessage(response);
        return;
    }
    
    uint32_t adminId;
    if (!userManager_->validateSession(sessionId, adminId)) {
        auto response = Protocol::createUpdateUserRoleResponse(false, "Invalid session");
        client->sendMessage(response);
        return;
    }
    
    // Check admin permission
    User admin;
    if (!userManager_->getUserById(adminId, admin) || admin.role != UserRole::ADMIN) {
        auto response = Protocol::createUpdateUserRoleResponse(false, "Permission denied");
        client->sendMessage(response);
        return;
    }
    
    // Case insensitive comparison
    std::transform(roleStr.begin(), roleStr.end(), roleStr.begin(), ::toupper);

    UserRole newRole = UserRole::AUTHOR;
    if (roleStr == "REVIEWER") newRole = UserRole::REVIEWER;
    else if (roleStr == "EDITOR") newRole = UserRole::EDITOR;
    else if (roleStr == "ADMIN") newRole = UserRole::ADMIN;
    else if (roleStr != "AUTHOR") {
        auto response = Protocol::createUpdateUserRoleResponse(false, "Invalid role");
        client->sendMessage(response);
        return;
    }
    
    if (userManager_->updateUserRole(targetUserId, newRole)) {
        auto response = Protocol::createUpdateUserRoleResponse(true, "User role updated successfully");
        client->sendMessage(response);
    } else {
        auto response = Protocol::createUpdateUserRoleResponse(false, "Failed to update user role");
        client->sendMessage(response);
    }
}

// ============================================================================
// 停用用户
// ============================================================================
void Server::handleDeactivateUser(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId, targetUserId;
    
    if (!Protocol::parseDeactivateUserRequest(request, sessionId, targetUserId)) {
        auto response = Protocol::createDeactivateUserResponse(false, "Invalid request");
        client->sendMessage(response);
        return;
    }
    
    uint32_t adminId;
    if (!userManager_->validateSession(sessionId, adminId)) {
        auto response = Protocol::createDeactivateUserResponse(false, "Invalid session");
        client->sendMessage(response);
        return;
    }
    
    // Check admin permission
    User admin;
    if (!userManager_->getUserById(adminId, admin) || admin.role != UserRole::ADMIN) {
        auto response = Protocol::createDeactivateUserResponse(false, "Permission denied");
        client->sendMessage(response);
        return;
    }
    
    if (userManager_->deactivateUser(targetUserId)) {
        auto response = Protocol::createDeactivateUserResponse(true, "User deactivated successfully");
        client->sendMessage(response);
    } else {
        auto response = Protocol::createDeactivateUserResponse(false, "Failed to deactivate user");
        client->sendMessage(response);
    }
}

// ============================================================================
// 系统备份
// ============================================================================
void Server::handleSystemBackup(Connection* client, const Protocol::Message& request) {
    uint32_t sessionId;
    
    if (!Protocol::parseSystemBackupRequest(request, sessionId)) {
        auto response = Protocol::createSystemBackupResponse(false, "Invalid request");
        client->sendMessage(response);
        return;
    }
    
    uint32_t adminId;
    if (!userManager_->validateSession(sessionId, adminId)) {
        auto response = Protocol::createSystemBackupResponse(false, "Invalid session");
        client->sendMessage(response);
        return;
    }
    
    // Check admin permission
    User admin;
    if (!userManager_->getUserById(adminId, admin) || admin.role != UserRole::ADMIN) {
        auto response = Protocol::createSystemBackupResponse(false, "Permission denied");
        client->sendMessage(response);
        return;
    }
    
    bool metaSuccess = reviewSystem_->saveMetadata();
    bool userSuccess = userManager_->saveToFile("users.dat");
    
    if (metaSuccess && userSuccess) {
        auto response = Protocol::createSystemBackupResponse(true, "System backup completed successfully");
        client->sendMessage(response);
    } else {
        auto response = Protocol::createSystemBackupResponse(false, "Backup partially failed");
        client->sendMessage(response);
    }
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
    uint32_t total_reviews = reviewSystem_->getReviewCount();
    
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
    
    response.header.length = response.payload.size();
    response.header.checksum = Protocol::calculateChecksum(response.payload);
    
    client->sendMessage(response);
}