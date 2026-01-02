#include "network/http_handler.h"
#include "user/user_manager.h"
#include "user/user.h"
#include "review/review_system.h"
#include "review/review.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>
#include <fstream>
#include <regex>

HttpHandler::HttpHandler(
    std::shared_ptr<UserManager> userManager,
    std::shared_ptr<ReviewSystem> reviewSystem,
    std::shared_ptr<Filesystem> filesystem
) : userManager_(userManager), reviewSystem_(reviewSystem), filesystem_(filesystem) {}

std::string HttpHandler::handleRequest(const std::string& method, const std::string& path,
                                      const std::string& body,
                                      const std::map<std::string, std::string>& headers) {
    try {
        std::cout << "📝 HTTP " << method << " " << path << std::endl;
        
        // 认证路由
        if (method == "POST" && path == "/api/auth/register") {
            return handleRegister(body);
        } else if (method == "POST" && path == "/api/auth/login") {
            return handleLogin(body);
        } else if (method == "POST" && path == "/api/auth/logout") {
            return handleLogout(body, headers);
        }
        
        // 文件管理路由
        else if (method == "GET" && path == "/api/files") {
            return handleGetFiles(headers);
        } else if (method == "POST" && path == "/api/files/upload") {
            return handleUploadFile(body, headers);
        } else if (method == "DELETE" && path.substr(0, 10) == "/api/files/") {
            return handleDeleteFile(path.substr(10), headers);
        }
        
        // 论文管理路由
        else if (method == "GET" && path == "/api/papers") {
            return handleGetPapers(headers);
        } else if (method == "POST" && path == "/api/papers") {
            return handleSubmitPaper(body, headers);
        } else if (method == "GET" && path == "/api/papers/my") {
            return handleGetMyPapers(headers);
        } else if (method == "GET" && path == "/api/papers/review") {
            return handleGetPapersToReview(headers);
        }
        
        // 评审路由
        else if (method == "POST" && path.substr(0, 15) == "/api/reviews/") {
            size_t paperIdStart = 15;
            size_t paperIdEnd = path.find('/', paperIdStart);
            std::string paperId = path.substr(paperIdStart, paperIdEnd - paperIdStart);
            return handleSubmitReview(paperId, body, headers);
        }
        
        // 统计路由
        else if (method == "GET" && path == "/api/statistics") {
            return handleGetStatistics();
        }
        
        // 主页
        else if (method == "GET" && path == "/") {
            return createHtmlResponse();
        }
        
        return createErrorResponse("404 Not Found");
    } catch (const std::exception& e) {
        std::cout << "❌ HTTP处理异常: " << e.what() << std::endl;
        return createErrorResponse(std::string("Internal Server Error: ") + e.what());
    }
}

// ============================================================================
// 用户认证
// ============================================================================

std::string HttpHandler::handleRegister(const std::string& body) {
    std::cout << "👤 处理用户注册请求" << std::endl;
    
    std::string username = parseJsonField(body, "username");
    std::string password = parseJsonField(body, "password");
    std::string role = parseJsonField(body, "role");
    
    if (username.empty() || password.empty() || role.empty()) {
        return createJsonResponse(false, "用户名、密码和角色不能为空");
    }
    
    // 角色验证
    UserRole userRole = UserRole::AUTHOR;
    if (role == "reviewer") userRole = UserRole::REVIEWER;
    else if (role == "editor") userRole = UserRole::EDITOR;
    else if (role != "author") {
        return createJsonResponse(false, "无效的角色，必须是 author、reviewer 或 editor");
    }
    
    if (userManager_->createUser(username, password, userRole)) {
        std::cout << "✅ 用户注册成功: " << username << " (" << role << ")" << std::endl;
        return createJsonResponse(true, "注册成功");
    } else {
        std::cout << "❌ 用户注册失败: " << username << " 已存在" << std::endl;
        return createJsonResponse(false, "用户名已存在");
    }
}

std::string HttpHandler::handleLogin(const std::string& body) {
    std::cout << "🔑 处理用户登录请求" << std::endl;
    
    std::string username = parseJsonField(body, "username");
    std::string password = parseJsonField(body, "password");
    
    if (username.empty() || password.empty()) {
        return createJsonResponse(false, "用户名和密码不能为空");
    }
    
    uint32_t userId;
    
    if (userManager_->authenticateUser(username, password, userId)) {
        uint32_t sessionId = userManager_->createSession(userId);
        
        User user;
        userManager_->getUserById(userId, user);
        std::string role = user.getRoleName();
        
        std::cout << "✅ 用户登录成功: " << username << " (ID: " << userId << ", Session: " << sessionId << ")" << std::endl;
        
        // 返回用户信息和会话
        std::map<std::string, std::string> data = {
            {"userId", std::to_string(userId)},
            {"username", username},
            {"role", role},
            {"sessionId", std::to_string(sessionId)}
        };
        
        return createJsonResponse(true, "登录成功", data);
    } else {
        std::cout << "❌ 用户登录失败: " << username << std::endl;
        return createJsonResponse(false, "用户名或密码错误");
    }
}

std::string HttpHandler::handleLogout(const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "🚪 处理用户登出请求" << std::endl;
    
    uint32_t sessionId, userId;
    if (validateAuth(headers, sessionId, userId)) {
        userManager_->invalidateSession(sessionId);
        std::cout << "✅ 用户登出成功: Session " << sessionId << std::endl;
        return createJsonResponse(true, "登出成功");
    } else {
        return createJsonResponse(false, "无效的会话");
    }
}

// ============================================================================
// 文件管理
// ============================================================================

std::string HttpHandler::handleGetFiles(const std::map<std::string, std::string>& headers) {
    std::cout << "📁 获取文件列表" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证");
    }
    
    auto files = filesystem_->listFiles();
    
    // 构造文件列表JSON
    std::ostringstream json;
    json << "{\"success\":true,\"data\":[";
    
    for (size_t i = 0; i < files.size(); ++i) {
        if (i > 0) json << ",";
        json << "{"
             << "\"name\":\"" << files[i].filename << "\","
             << "\"size\":" << files[i].size << ","
             << "\"timestamp\":" << files[i].timestamp
             << "}";
    }
    
    json << "]}";
    return json.str();
}

std::string HttpHandler::handleUploadFile(const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "📤 处理文件上传" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证");
    }
    
    // 简单的文件上传处理
    std::string filename = parseJsonField(body, "filename");
    std::string content = parseJsonField(body, "content");
    
    if (filename.empty()) {
        return createJsonResponse(false, "文件名不能为空");
    }
    
    // 这里应该实现实际的文件写入逻辑
    std::cout << "📝 文件上传: " << filename << " (" << content.length() << " bytes)" << std::endl;
    
    return createJsonResponse(true, "文件上传成功");
}

std::string HttpHandler::handleDeleteFile(const std::string& filename, const std::map<std::string, std::string>& headers) {
    std::cout << "🗑️ 删除文件: " << filename << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证");
    }
    
    // 这里应该实现实际的文件删除逻辑
    std::cout << "✅ 文件删除: " << filename << std::endl;
    
    return createJsonResponse(true, "文件删除成功");
}

// ============================================================================
// 论文管理
// ============================================================================

std::string HttpHandler::handleGetPapers(const std::map<std::string, std::string>& headers) {
    std::cout << "📄 获取所有论文" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证");
    }
    
    // 获取所有论文
    auto papers = reviewSystem_->getAllPapers();
    
    std::ostringstream json;
    json << "{\"success\":true,\"data\":[";
    
    for (size_t i = 0; i < papers.size(); ++i) {
        if (i > 0) json << ",";
        json << "{"
             << "\"paperId\":" << papers[i].paperId << ","
             << "\"title\":\"" << escapeJson(papers[i].title) << "\","
             << "\"status\":\"" << papers[i].getStatusName() << "\","
             << "\"submissionTime\":" << papers[i].submissionTime
             << "}";
    }
    
    json << "]}";
    return json.str();
}

std::string HttpHandler::handleGetMyPapers(const std::map<std::string, std::string>& headers) {
    std::cout << "📄 获取我的论文" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证");
    }
    
    // 获取用户的论文
    auto papers = reviewSystem_->getPapersByAuthor(userId);
    
    std::ostringstream json;
    json << "{\"success\":true,\"data\":[";
    
    for (size_t i = 0; i < papers.size(); ++i) {
        if (i > 0) json << ",";
        json << "{"
             << "\"paperId\":" << papers[i].paperId << ","
             << "\"title\":\"" << escapeJson(papers[i].title) << "\","
             << "\"status\":\"" << papers[i].getStatusName() << "\","
             << "\"submissionTime\":" << papers[i].submissionTime
             << "}";
    }
    
    json << "]}";
    return json.str();
}

std::string HttpHandler::handleGetPapersToReview(const std::map<std::string, std::string>& headers) {
    std::cout << "📄 获取待审论文" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证");
    }
    
    // 获取待审的论文
    auto papers = reviewSystem_->getPapersToReview(userId);
    
    std::ostringstream json;
    json << "{\"success\":true,\"data\":[";
    
    for (size_t i = 0; i < papers.size(); ++i) {
        if (i > 0) json << ",";
        json << "{"
             << "\"paperId\":" << papers[i].paperId << ","
             << "\"title\":\"" << escapeJson(papers[i].title) << "\","
             << "\"status\":\"" << papers[i].getStatusName() << "\","
             << "\"submissionTime\":" << papers[i].submissionTime
             << "}";
    }
    
    json << "]}";
    return json.str();
}

std::string HttpHandler::handleSubmitPaper(const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "📝 提交论文" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证");
    }
    
    std::string title = parseJsonField(body, "title");
    std::string abstract = parseJsonField(body, "abstract");
    
    if (title.empty()) {
        return createJsonResponse(false, "论文标题不能为空");
    }
    
    // 从JSON中获取文件数据
    std::string fileDataStr = parseJsonField(body, "fileData");
    std::vector<char> fileData(fileDataStr.begin(), fileDataStr.end());
    
    uint32_t paperId = reviewSystem_->submitPaper(userId, title, abstract, fileData);
    
    if (paperId > 0) {
        std::cout << "✅ 论文提交成功: ID " << paperId << std::endl;
        std::map<std::string, std::string> data = {{"paperId", std::to_string(paperId)}};
        return createJsonResponse(true, "论文提交成功", data);
    } else {
        return createJsonResponse(false, "论文提交失败");
    }
}

// ============================================================================
// 评审管理
// ============================================================================

std::string HttpHandler::handleSubmitReview(const std::string& paperIdStr, const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "✍️ 提交评审 - 论文ID: " << paperIdStr << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证");
    }
    
    uint32_t paperId = std::stoi(paperIdStr);
    std::string decision = parseJsonField(body, "decision");
    std::string comments = parseJsonField(body, "comments");
    int confidence = std::stoi(parseJsonField(body, "confidence"));
    
    // 解析决定
    ReviewDecision reviewDecision = ReviewDecision::BORDERLINE;
    if (decision == "STRONG_ACCEPT") reviewDecision = ReviewDecision::STRONG_ACCEPT;
    else if (decision == "ACCEPT") reviewDecision = ReviewDecision::ACCEPT;
    else if (decision == "WEAK_ACCEPT") reviewDecision = ReviewDecision::WEAK_ACCEPT;
    else if (decision == "WEAK_REJECT") reviewDecision = ReviewDecision::WEAK_REJECT;
    else if (decision == "REJECT") reviewDecision = ReviewDecision::REJECT;
    else if (decision == "STRONG_REJECT") reviewDecision = ReviewDecision::STRONG_REJECT;
    
    uint32_t reviewId = reviewSystem_->submitReview(userId, paperId, reviewDecision, confidence, comments);
    
    if (reviewId > 0) {
        std::cout << "✅ 评审提交成功: ID " << reviewId << std::endl;
        std::map<std::string, std::string> data = {{"reviewId", std::to_string(reviewId)}};
        return createJsonResponse(true, "评审提交成功", data);
    } else {
        return createJsonResponse(false, "评审提交失败");
    }
}

// ============================================================================
// 统计信息
// ============================================================================

std::string HttpHandler::handleGetStatistics() {
    std::cout << "📊 获取统计信息" << std::endl;
    
    auto paperStats = reviewSystem_->getStatistics();
    
    // 计算总数
    uint32_t total_papers = 0;
    for (const auto& [status, count] : paperStats) {
        total_papers += count;
    }
    
    std::map<std::string, std::string> data = {
        {"total_papers", std::to_string(total_papers)},
        {"total_users", std::to_string(userManager_->getUserCount())},
        {"total_reviews", "0"} // 这里应该实现实际计算
    };
    
    return createJsonResponse(true, "统计信息获取成功", data);
}

// ============================================================================
// 主页
// ============================================================================

std::string HttpHandler::createHtmlResponse() {
    return R"(<!DOCTYPE html>
<html>
<head>
    <title>Peer Review System API</title>
    <meta charset="UTF-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .endpoint { background: #f5f5f5; padding: 10px; margin: 10px 0; border-radius: 5px; }
        .method { font-weight: bold; }
        .get { color: #28a745; }
        .post { color: #007bff; }
        .delete { color: #dc3545; }
    </style>
</head>
<body>
    <h1>📝 Peer Review System API</h1>
    <p>API服务器运行正常！</p>
    
    <h2>🔐 认证接口</h2>
    <div class="endpoint"><span class="method post">POST</span> /api/auth/register - 用户注册</div>
    <div class="endpoint"><span class="method post">POST</span> /api/auth/login - 用户登录</div>
    <div class="endpoint"><span class="method post">POST</span> /api/auth/logout - 用户登出</div>
    
    <h2>📄 论文管理</h2>
    <div class="endpoint"><span class="method get">GET</span> /api/papers - 获取所有论文</div>
    <div class="endpoint"><span class="method get">GET</span> /api/papers/my - 获取我的论文</div>
    <div class="endpoint"><span class="method get">GET</span> /api/papers/review - 获取待审论文</div>
    <div class="endpoint"><span class="method post">POST</span> /api/papers - 提交论文</div>
    
    <h2>📁 文件管理</h2>
    <div class="endpoint"><span class="method get">GET</span> /api/files - 获取文件列表</div>
    <div class="endpoint"><span class="method post">POST</span> /api/files/upload - 上传文件</div>
    <div class="endpoint"><span class="method delete">DELETE</span> /api/files/{filename} - 删除文件</div>
    
    <h2>📊 统计信息</h2>
    <div class="endpoint"><span class="method get">GET</span> /api/statistics - 获取统计信息</div>
</body>
</html>)";
}

// ============================================================================
// 辅助函数
// ============================================================================

std::string HttpHandler::createJsonResponse(bool success, const std::string& message, const std::map<std::string, std::string>& data) {
    std::ostringstream json;
    json << "{\"success\":" << (success ? "true" : "false") << ",\"message\":\"" << escapeJson(message) << "\"";
    
    if (!data.empty()) {
        json << ",\"data\":{";
        bool first = true;
        for (const auto& [key, value] : data) {
            if (!first) json << ",";
            json << "\"" << key << "\":\"" << escapeJson(value) << "\"";
            first = false;
        }
        json << "}";
    }
    
    json << "}";
    
    return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + json.str();
}

std::string HttpHandler::createErrorResponse(const std::string& error) {
    return std::string("HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n\r\n") +
           "{\"success\":false,\"message\":\"" + escapeJson(error) + "\"}";
}

std::string HttpHandler::parseJsonField(const std::string& json, const std::string& field) {
    std::string pattern = "\"" + field + "\"\\s*:\\s*\"([^\"]*)\"";
    std::regex regex(pattern);
    std::smatch match;
    
    if (std::regex_search(json, match, regex)) {
        return match[1].str();
    }
    
    return "";
}

std::string HttpHandler::escapeJson(const std::string& str) {
    std::string result = str;
    // 转义双引号
    for (size_t i = 0; i < result.length(); ++i) {
        if (result[i] == '"') {
            result.insert(result.begin() + i, '\\');
            ++i;
        } else if (result[i] == '\n') {
            result.replace(i, 1, "\\n");
            ++i;
        } else if (result[i] == '\r') {
            result.replace(i, 1, "\\r");
            ++i;
        }
    }
    return result;
}

bool HttpHandler::validateAuth(const std::map<std::string, std::string>& headers, uint32_t& sessionId, uint32_t& userId) {
    auto authHeader = headers.find("Authorization");
    if (authHeader == headers.end()) {
        return false;
    }
    
    // 简单的Bearer token解析
    std::string auth = authHeader->second;
    if (auth.substr(0, 7) == "Bearer ") {
        try {
            sessionId = std::stoul(auth.substr(7));
            return userManager_->validateSession(sessionId, userId);
        } catch (const std::exception&) {
            return false;
        }
    }
    
    return false;
}