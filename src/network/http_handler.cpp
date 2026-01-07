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
#include <iomanip>
#include <sys/resource.h>
#include <sys/time.h>
#include <mutex>
#include <filesystem>
#include <vector>
#include <algorithm>

// Static member initialization
std::vector<std::string> HttpHandler::systemLogs_;
std::mutex logMutex;

void HttpHandler::addSystemLog(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now), "%H:%M:%S");
    
    std::string logEntry = "[" + ss.str() + "] [" + level + "] " + message;
    systemLogs_.push_back(logEntry);
    
    // Keep only last 100 logs
    if (systemLogs_.size() > 100) {
        systemLogs_.erase(systemLogs_.begin());
    }
}

// Helper function for Base64 decoding
static const std::string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

static bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

static std::vector<char> base64Decode(const std::string& encoded_string) {
  int in_len = encoded_string.size();
  int i = 0;
  int j = 0;
  int in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::vector<char> ret;

  while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_]; in_++;
    if (i ==4) {
      for (i = 0; i <4; i++)
        char_array_4[i] = base64_chars.find(char_array_4[i]);

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; (i < 3); i++)
        ret.push_back(char_array_3[i]);
      i = 0;
    }
  }

  if (i) {
    for (j = i; j <4; j++)
      char_array_4[j] = 0;

    for (j = 0; j <4; j++)
      char_array_4[j] = base64_chars.find(char_array_4[j]);

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; (j < i - 1); j++) ret.push_back(char_array_3[j]);
  }

  return ret;
}

HttpHandler::HttpHandler(
    std::shared_ptr<UserManager> userManager,
    std::shared_ptr<ReviewSystem> reviewSystem,
    std::shared_ptr<Filesystem> filesystem
) : userManager_(userManager), reviewSystem_(reviewSystem), filesystem_(filesystem) {}

std::string HttpHandler::handleRequest(const std::string& method, const std::string& path,
                                      const std::string& body,
                                      const std::map<std::string, std::string>& reqHeaders) {
    try {
        std::cout << "📝 HTTP " << method << " " << path << std::endl;
        
        // Parse query string
        std::string cleanPath = path;
        std::map<std::string, std::string> queryParams;
        size_t queryPos = path.find('?');
        if (queryPos != std::string::npos) {
            cleanPath = path.substr(0, queryPos);
            std::string queryStr = path.substr(queryPos + 1);
            
            std::stringstream ss(queryStr);
            std::string segment;
            while (std::getline(ss, segment, '&')) {
                size_t equalPos = segment.find('=');
                if (equalPos != std::string::npos) {
                    std::string key = segment.substr(0, equalPos);
                    std::string value = segment.substr(equalPos + 1);
                    queryParams[key] = value;
                }
            }
        }

        // Create mutable headers and inject sessionId if present
        std::map<std::string, std::string> headers = reqHeaders;
        if (queryParams.count("sessionId")) {
            headers["Authorization"] = "Bearer " + queryParams["sessionId"];
        }
        
        // 处理CORS预检请求
        if (method == "OPTIONS") {
            return "HTTP/1.1 200 OK\r\n" + createCorsHeaders(headers) + "\r\n";
        }
        
        // 认证路由
        if (method == "POST" && cleanPath == "/api/auth/register") {
            return handleRegister(body, headers);
        } else if (method == "POST" && cleanPath == "/api/auth/login") {
            return handleLogin(body, headers);
        } else if (method == "POST" && cleanPath == "/api/auth/logout") {
            return handleLogout(body, headers);
        } else if (method == "GET" && cleanPath == "/api/auth/heartbeat") {
            return handleHeartbeat(headers);
        }
        
        // 文件管理路由
        else if (method == "GET" && cleanPath == "/api/files") {
            return handleGetFiles(headers);
        } else if (method == "POST" && cleanPath == "/api/files/upload") {
            return handleUploadFile(body, headers);
        } else if (method == "DELETE" && cleanPath.substr(0, 10) == "/api/files/") {
            return handleDeleteFile(cleanPath.substr(10), headers);
        }
        
        // 论文管理路由
        else if (method == "GET" && cleanPath == "/api/papers") {
            return handleGetPapers(headers);
        } else if (method == "POST" && cleanPath == "/api/papers") {
            return handleSubmitPaper(body, headers);
        } else if (method == "GET" && cleanPath == "/api/papers/my") {
            return handleGetMyPapers(headers);
        } else if (method == "GET" && cleanPath == "/api/papers/review") {
            return handleGetPapersToReview(headers);        } else if (method == "POST" && cleanPath == "/api/papers/assign") {
            return handleAssignReviewer(body, headers);
        } else if (method == "GET" && cleanPath == "/api/reviews/progress") {
            return handleGetReviewProgress(headers);
        } else if (method == "POST" && cleanPath.find("/api/papers/") == 0 && cleanPath.find("/revision") != std::string::npos) {
            // /api/papers/{id}/revision
            size_t idStart = 12; // "/api/papers/".length()
            size_t idEnd = cleanPath.find('/', idStart);
            std::string paperId = cleanPath.substr(idStart, idEnd - idStart);
            return handleUploadRevision(paperId, body, headers);
        } else if (method == "POST" && cleanPath.find("/api/papers/") == 0 && cleanPath.find("/update_file") != std::string::npos) {
            // /api/papers/{id}/update_file
            size_t idStart = 12; // "/api/papers/".length()
            size_t idEnd = cleanPath.find('/', idStart);
            std::string paperId = cleanPath.substr(idStart, idEnd - idStart);
            return handleUpdatePaperFile(paperId, body, headers);
        } else if (method == "GET" && cleanPath.find("/api/papers/") == 0 && cleanPath.find("/download") != std::string::npos) {
            // /api/papers/{id}/download
            size_t idStart = 12;
            size_t idEnd = cleanPath.find('/', idStart);
            std::string paperId = cleanPath.substr(idStart, idEnd - idStart);
            return handleDownloadPaper(paperId, headers);
        } else if (method == "GET" && cleanPath.find("/api/reviews/") == 0 && cleanPath.find("/download") != std::string::npos) {
            // /api/reviews/{id}/download
            size_t idStart = 13;
            size_t idEnd = cleanPath.find('/', idStart);
            std::string reviewId = cleanPath.substr(idStart, idEnd - idStart);
            return handleDownloadReviewAttachment(reviewId, headers);
        } else if (method == "POST" && cleanPath.find("/api/papers/") == 0 && cleanPath.find("/decision") != std::string::npos) {
            // /api/papers/{id}/decision
            size_t idStart = 12;
            size_t idEnd = cleanPath.find('/', idStart);
            std::string paperId = cleanPath.substr(idStart, idEnd - idStart);
            return handleMakeDecision(paperId, body, headers);
        }
        
        // 评审路由
        else if (method == "POST" && cleanPath.find("/api/reviews/") == 0) {
            size_t paperIdStart = 13;
            size_t paperIdEnd = cleanPath.find('/', paperIdStart);
            std::string paperId = cleanPath.substr(paperIdStart, paperIdEnd - paperIdStart);
            return handleSubmitReview(paperId, body, headers);
        }
        
        // 统计路由
        else if (method == "GET" && cleanPath == "/api/statistics") {
            return handleGetStatistics(headers);
        }
        
        // 用户管理路由
        else if (method == "GET" && cleanPath == "/api/user/profile") {
            return handleGetUserProfile(headers);
        } else if (method == "GET" && cleanPath.substr(0, 12) == "/api/system/") {
            if (cleanPath == "/api/system/stats") {
                return handleGetSystemStats("24h", headers);
            } else if (cleanPath == "/api/system/online-users") {
                return handleGetOnlineUsers(headers);
            } else if (cleanPath == "/api/system/logs") {
                return handleGetSystemLogs(headers);
            }
        } else if (method == "GET" && cleanPath == "/api/users") {
            std::string page = queryParams.count("page") ? queryParams["page"] : "1";
            std::string limit = queryParams.count("limit") ? queryParams["limit"] : "10";
            std::string search = queryParams.count("search") ? queryParams["search"] : "";
            std::string role = queryParams.count("role") ? queryParams["role"] : "";
            return handleGetUsers(page, limit, search, role, headers);
        } else if (method == "PUT" && cleanPath.find("/api/users/") == 0 && cleanPath.find("/role") != std::string::npos) {
            // /api/users/{id}/role
            size_t idStart = 11; // "/api/users/".length()
            size_t idEnd = cleanPath.find('/', idStart);
            std::string userId = cleanPath.substr(idStart, idEnd - idStart);
            return handleUpdateUserRole(userId, body, headers);
        } else if (method == "POST" && cleanPath.find("/api/users/") == 0 && cleanPath.find("/deactivate") != std::string::npos) {
            // /api/users/{id}/deactivate
            size_t idStart = 11;
            size_t idEnd = cleanPath.find('/', idStart);
            std::string userId = cleanPath.substr(idStart, idEnd - idStart);
            return handleDeactivateUser(userId, headers);
        } else if (method == "POST" && cleanPath == "/api/system/backup") {
            return handleSystemBackup(headers);
        } else if (method == "POST" && cleanPath == "/api/admin/snapshots") {
            return handleCreateSnapshot(headers);
        } else if (method == "GET" && cleanPath == "/api/admin/snapshots") {
            return handleListSnapshots(headers);
        } else if (method == "POST" && cleanPath == "/api/admin/snapshots/restore") {
            return handleRestoreSnapshot(body, headers);
        } else if (method == "POST" && cleanPath == "/api/admin/snapshots/delete") {
            return handleDeleteSnapshot(body, headers);
        } else if (method == "GET" && cleanPath == "/api/overview") {
            return handleGetOverview(headers);
        }
        
        // 主页
        else if (method == "GET" && cleanPath == "/") {
            return handleStaticFile("/index.html");
        }
        
        // 静态文件
        else if (method == "GET" && cleanPath.find("/api/") != 0) {
            return handleStaticFile(cleanPath);
        }
        
        return createErrorResponse("404 Not Found", headers);
    } catch (const std::exception& e) {
        std::cout << "❌ HTTP处理异常: " << e.what() << std::endl;
        return createErrorResponse(std::string("Internal Server Error: ") + e.what(), reqHeaders);
    } catch (...) {
        std::cout << "❌ HTTP处理未知异常" << std::endl;
        return createErrorResponse("Internal Server Error: Unknown Exception", reqHeaders);
    }
}

// ============================================================================
// 用户认证
// ============================================================================

std::string HttpHandler::handleRegister(const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "👤 处理用户注册请求" << std::endl;
    
    std::string username = parseJsonField(body, "username");
    std::string password = parseJsonField(body, "password");
    std::string role = parseJsonField(body, "role");
    
    if (username.empty() || password.empty() || role.empty()) {
        return createJsonResponse(false, "用户名、密码和角色不能为空", {}, headers);
    }
    
    // 角色验证 (支持前端发送的大写和小写角色值)
    UserRole userRole = UserRole::AUTHOR;
    if (role == "REVIEWER" || role == "reviewer") userRole = UserRole::REVIEWER;
    else if (role == "EDITOR" || role == "editor") userRole = UserRole::EDITOR;
    else if (role == "ADMIN" || role == "admin") userRole = UserRole::ADMIN;
    else if (role != "AUTHOR" && role != "author") {
        return createJsonResponse(false, "无效的角色，必须是 author, reviewer, editor 或 admin", {}, headers);
    }
    
    if (userManager_->createUser(username, password, userRole)) {
        std::cout << "✅ 用户注册成功: " << username << " (" << role << ")" << std::endl;
        addSystemLog("INFO", "New user registered: " + username + " (" + role + ")");
        return createJsonResponse(true, "注册成功", {}, headers);
    } else {
        std::cout << "❌ 用户注册失败: " << username << " 已存在" << std::endl;
        addSystemLog("WARN", "Registration failed: username " + username + " already exists");
        return createJsonResponse(false, "用户名已存在", {}, headers);
    }
}

std::string HttpHandler::handleLogin(const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "🔑 处理用户登录请求" << std::endl;
    
    std::string username = parseJsonField(body, "username");
    std::string password = parseJsonField(body, "password");
    
    if (username.empty() || password.empty()) {
        return createJsonResponse(false, "用户名和密码不能为空", {}, headers);
    }
    
    uint32_t userId;
    
    if (userManager_->authenticateUser(username, password, userId)) {
        uint32_t sessionId = userManager_->createSession(userId);
        
        User user;
        userManager_->getUserById(userId, user);
        std::string role = user.getRoleName();
        
        std::cout << "✅ 用户登录成功: " << username << " (ID: " << userId << ", Session: " << sessionId << ")" << std::endl;
        addSystemLog("INFO", "User logged in: " + username + " (ID: " + std::to_string(userId) + ")");
        
        // 返回用户信息和会话
        std::map<std::string, std::string> data = {
            {"userId", std::to_string(userId)},
            {"username", username},
            {"role", role},
            {"sessionId", std::to_string(sessionId)}
        };
        
        return createJsonResponse(true, "登录成功", data, headers);
    } else {
        std::cout << "❌ 用户登录失败: " << username << std::endl;
        addSystemLog("WARN", "Login failed for user: " + username);
        return createJsonResponse(false, "用户名或密码错误", {}, headers);
    }
}

std::string HttpHandler::handleHeartbeat(const std::map<std::string, std::string>& headers) {
    // Heartbeat just needs to validate auth, which updates lastActivityTime
    uint32_t sessionId, userId;
    if (validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(true, "Heartbeat received", {}, headers);
    } else {
        return createJsonResponse(false, "Invalid session", {}, headers);
    }
}

std::string HttpHandler::handleLogout(const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "🚪 处理用户登出请求" << std::endl;
    
    uint32_t sessionId, userId;
    if (validateAuth(headers, sessionId, userId)) {
        userManager_->invalidateSession(sessionId);
        std::cout << "✅ 用户登出成功: Session " << sessionId << std::endl;
        addSystemLog("INFO", "User logged out: Session " + std::to_string(sessionId));
        return createJsonResponse(true, "登出成功", {}, headers);
    } else {
        return createJsonResponse(false, "无效的会话", {}, headers);
    }
}

// ============================================================================
// 文件管理
// ============================================================================

std::string HttpHandler::handleGetFiles(const std::map<std::string, std::string>& headers) {
    std::cout << "📁 获取文件列表" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    auto files = filesystem_->listFiles();
    
    // 构造文件列表数据
    std::map<std::string, std::string> data;
    
    for (size_t i = 0; i < files.size(); ++i) {
        data[std::to_string(i)] = files[i].filename; // 简化的数据结构
    }
    
    return createJsonResponse(true, "文件列表获取成功", data, headers);
}

std::string HttpHandler::handleUploadFile(const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "📤 处理文件上传" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    // 简单的文件上传处理
    std::string filename = parseJsonField(body, "filename");
    std::string content = parseJsonField(body, "content");
    
    if (filename.empty()) {
        return createJsonResponse(false, "文件名不能为空", {}, headers);
    }
    
    // 这里应该实现实际的文件写入逻辑
    std::cout << "📝 文件上传: " << filename << " (" << content.length() << " bytes)" << std::endl;
    
    std::map<std::string, std::string> data = {
        {"filename", filename},
        {"size", std::to_string(content.length())}
    };
    
    return createJsonResponse(true, "文件上传成功", data, headers);
}

std::string HttpHandler::handleDeleteFile(const std::string& filename, const std::map<std::string, std::string>& headers) {
    std::cout << "🗑️ 删除文件: " << filename << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    // 这里应该实现实际的文件删除逻辑
    std::cout << "✅ 文件删除: " << filename << std::endl;
    
    std::map<std::string, std::string> data = {
        {"filename", filename}
    };
    
    return createJsonResponse(true, "文件删除成功", data, headers);
}

// ============================================================================
// 论文管理
// ============================================================================

std::string HttpHandler::handleGetPapers(const std::map<std::string, std::string>& headers) {
    std::cout << "📄 获取所有论文" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    // 获取所有论文
    auto papers = reviewSystem_->getAllPapers();
    
    std::ostringstream json;
    json << "{\"success\":true,\"data\":{\"count\":\"" << papers.size() << "\",\"papers\":[";
    
    for (size_t i = 0; i < papers.size(); ++i) {
        if (i > 0) json << ",";
        json << "{"
             << "\"id\":" << papers[i].paperId << ","
             << "\"title\":\"" << escapeJson(papers[i].title) << "\","
             << "\"status\":\"" << papers[i].getStatusName() << "\","
             << "\"author\":\"" << papers[i].authorIds[0] << "\"," // 简化：只显示第一个作者ID
             << "\"timestamp\":" << papers[i].submissionTime << ","
             << "\"abstract\":\"" << escapeJson(papers[i].abstract) << "\",";
             
        json << "\"assignedReviewers\":[";
        for (size_t j = 0; j < papers[i].assignedReviewers.size(); ++j) {
            if (j > 0) json << ",";
            json << papers[i].assignedReviewers[j];
        }
        json << "]";
        
        json << "}";
    }
    
    json << "]}}";
    return "HTTP/1.1 200 OK\r\n" + createCorsHeaders(headers) + "Content-Type: application/json\r\n\r\n" + json.str();
}

std::string HttpHandler::handleGetMyPapers(const std::map<std::string, std::string>& headers) {
    std::cout << "📄 获取我的论文" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    // 获取用户的论文
    auto papers = reviewSystem_->getPapersByAuthor(userId);
    
    std::ostringstream json;
    json << "{\"success\":true,\"data\":{\"papers\":[";
    
    for (size_t i = 0; i < papers.size(); ++i) {
        if (i > 0) json << ",";
        
        // 获取评审意见 (如果论文状态是 ACCEPTED 或 REJECTED)
        std::string reviewsJson = "[]";
        if (papers[i].status == PaperStatus::ACCEPTED || papers[i].status == PaperStatus::REJECTED) {
            auto reviews = reviewSystem_->getReviewsForPaper(papers[i].paperId);
            std::ostringstream rJson;
            rJson << "[";
            for (size_t j = 0; j < reviews.size(); ++j) {
                if (j > 0) rJson << ",";
                rJson << "{"
                      << "\"reviewId\":" << reviews[j].reviewId << ","
                      << "\"decision\":\"" << reviews[j].getDecisionName() << "\","
                      << "\"confidence\":" << reviews[j].confidenceScore << ","
                      << "\"comments\":\"" << escapeJson(reviews[j].comments) << "\","
                      << "\"hasFile\":" << (reviews[j].filepath.empty() ? "false" : "true")
                      << "}";
            }
            rJson << "]";
            reviewsJson = rJson.str();
        }

        json << "{"
             << "\"id\":" << papers[i].paperId << ","
             << "\"title\":\"" << escapeJson(papers[i].title) << "\","
             << "\"status\":\"" << papers[i].getStatusName() << "\","
             << "\"author\":\"" << papers[i].authorIds[0] << "\","
             << "\"timestamp\":" << papers[i].submissionTime << ","
             << "\"abstract\":\"" << escapeJson(papers[i].abstract) << "\","
             << "\"reviews\":" << reviewsJson
             << "}";
    }
    
    json << "]}}";
    return "HTTP/1.1 200 OK\r\n" + createCorsHeaders(headers) + "Content-Type: application/json\r\n\r\n" + json.str();
}

std::string HttpHandler::handleGetPapersToReview(const std::map<std::string, std::string>& headers) {
    std::cout << "📄 获取待审论文" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    // 获取分配给该审稿人的所有论文
    auto papers = reviewSystem_->getPapersForReviewer(userId);
    
    std::ostringstream json;
    json << "{\"success\":true,\"data\":[";
    
    for (size_t i = 0; i < papers.size(); ++i) {
        if (i > 0) json << ",";
        
        // 检查是否已评审
        bool reviewed = false;
        std::string myReviewJson = "null";
        auto reviews = reviewSystem_->getReviewsForPaper(papers[i].paperId);
        for(const auto& r : reviews) {
            if(r.reviewerId == userId) {
                reviewed = true;
                std::ostringstream rJson;
                rJson << "{"
                      << "\"decision\":\"" << r.getDecisionName() << "\","
                      << "\"confidence\":" << r.confidenceScore << ","
                      << "\"comments\":\"" << escapeJson(r.comments) << "\""
                      << "}";
                myReviewJson = rJson.str();
                break;
            }
        }
        
        json << "{"
             << "\"paperId\":" << papers[i].paperId << ","
             << "\"title\":\"" << escapeJson(papers[i].title) << "\","
             << "\"status\":\"" << papers[i].getStatusName() << "\","
             << "\"submissionTime\":" << papers[i].submissionTime << ","
             << "\"reviewed\":" << (reviewed ? "true" : "false") << ","
             << "\"myReview\":" << myReviewJson
             << "}";
    }
    
    json << "]}";
    return "HTTP/1.1 200 OK\r\n" + createCorsHeaders(headers) + "Content-Type: application/json\r\n\r\n" + json.str();
}

std::string HttpHandler::handleSubmitPaper(const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "📝 提交论文 (Body size: " << body.size() << ")" << std::endl;
    if (body.size() < 1000) {
        std::cout << "Body content: " << body << std::endl;
    } else {
        std::cout << "Body content (first 500 chars): " << body.substr(0, 500) << std::endl;
    }
    
    uint32_t sessionId, userId;
    std::cout << "   Validating auth..." << std::endl;
    if (!validateAuth(headers, sessionId, userId)) {
        std::cout << "   Auth failed" << std::endl;
        return createJsonResponse(false, "未认证", {}, headers);
    }
    std::cout << "   Auth success. UserID: " << userId << std::endl;
    
    std::cout << "   Parsing title..." << std::endl;
    std::string title = parseJsonField(body, "title");
    std::cout << "   Parsing abstract..." << std::endl;
    std::string abstract = parseJsonField(body, "abstract");
    
    if (title.empty()) {
        return createJsonResponse(false, "论文标题不能为空", {}, headers);
    }
    
    // 从JSON中获取文件数据
    std::cout << "   Parsing fileData..." << std::endl;
    std::string fileDataStr = parseJsonField(body, "fileData");
    std::cout << "   File data size (Base64): " << fileDataStr.size() << std::endl;
    
    // Decode Base64
    std::vector<char> fileData = base64Decode(fileDataStr);
    std::cout << "   File data size (Decoded): " << fileData.size() << std::endl;
    
    std::cout << "   Submitting to ReviewSystem..." << std::endl;
    uint32_t paperId = reviewSystem_->submitPaper(userId, title, abstract, fileData);
    std::cout << "   ReviewSystem returned: " << paperId << std::endl;
    
    if (paperId > 0) {
        std::cout << "✅ 论文提交成功: ID " << paperId << std::endl;
        addSystemLog("INFO", "Paper submitted: " + title + " (ID: " + std::to_string(paperId) + ") by User " + std::to_string(userId));
        std::map<std::string, std::string> data = {{"paperId", std::to_string(paperId)}};
        return createJsonResponse(true, "论文提交成功", data, headers);
    } else {
        addSystemLog("ERROR", "Paper submission failed for User " + std::to_string(userId));
        return createJsonResponse(false, "论文提交失败", {}, headers);
    }
}

// ============================================================================
// 评审管理
// ============================================================================

std::string HttpHandler::handleSubmitReview(const std::string& paperIdStr, const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "✍️ 提交评审 - 论文ID: " << paperIdStr << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    uint32_t paperId = std::stoi(paperIdStr);
    std::string decision = parseJsonField(body, "decision");
    std::string comments = parseJsonField(body, "comments");
    int confidence = std::stoi(parseJsonField(body, "confidence"));
    
    // 解析文件数据
    std::string fileDataStr = parseJsonField(body, "fileData");
    std::string filename = parseJsonField(body, "filename");
    
    std::cout << "   Filename from JSON: '" << filename << "'" << std::endl;
    
    std::vector<char> fileData;
    if (!fileDataStr.empty()) {
        fileData = base64Decode(fileDataStr);
    }

    // 解析决定
    ReviewDecision reviewDecision = ReviewDecision::BORDERLINE;
    if (decision == "STRONG_ACCEPT") reviewDecision = ReviewDecision::STRONG_ACCEPT;
    else if (decision == "ACCEPT") reviewDecision = ReviewDecision::ACCEPT;
    else if (decision == "WEAK_ACCEPT") reviewDecision = ReviewDecision::WEAK_ACCEPT;
    else if (decision == "WEAK_REJECT") reviewDecision = ReviewDecision::WEAK_REJECT;
    else if (decision == "REJECT") reviewDecision = ReviewDecision::REJECT;
    else if (decision == "STRONG_REJECT") reviewDecision = ReviewDecision::STRONG_REJECT;
    
    uint32_t reviewId = reviewSystem_->submitReview(userId, paperId, reviewDecision, confidence, comments, fileData, filename);
    
    if (reviewId > 0) {
        std::cout << "✅ 评审提交成功: ID " << reviewId << std::endl;
        addSystemLog("INFO", "Review submitted for Paper " + std::to_string(paperId) + " by User " + std::to_string(userId));
        std::map<std::string, std::string> data = {{"reviewId", std::to_string(reviewId)}};
        return createJsonResponse(true, "评审提交成功", data, headers);
    } else {
        addSystemLog("ERROR", "Review submission failed for Paper " + std::to_string(paperId) + " by User " + std::to_string(userId));
        return createJsonResponse(false, "评审提交失败", {}, headers);
    }
}

std::string HttpHandler::handleAssignReviewer(const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "👉 分配评审" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    // 检查是否是编辑
    User user;
    if (!userManager_->getUserById(userId, user) || user.role != UserRole::EDITOR) {
        return createJsonResponse(false, "权限不足，只有编辑可以分配评审", {}, headers);
    }
    
    std::string paperIdStr = parseJsonField(body, "paperId");
    std::string reviewerIdStr = parseJsonField(body, "reviewerId");
    std::string mode = parseJsonField(body, "mode");
    
    if (paperIdStr.empty()) {
        return createJsonResponse(false, "论文ID不能为空", {}, headers);
    }
    
    uint32_t paperId = std::stoi(paperIdStr);

    // Auto Assign Mode
    if (mode == "auto") {
        if (reviewSystem_->autoAssignReviewers(paperId)) {
            std::cout << "✅ 自动分配成功: Paper " << paperId << std::endl;
            return createJsonResponse(true, "自动分配成功", {}, headers);
        } else {
            return createJsonResponse(false, "自动分配失败：没有找到满足条件的评审人", {}, headers);
        }
    }
    
    // Manual Assign Mode
    if (reviewerIdStr.empty()) {
        return createJsonResponse(false, "评审人ID不能为空", {}, headers);
    }
    
    uint32_t reviewerId = std::stoi(reviewerIdStr);
    
    // Check if already assigned
    std::vector<uint32_t> assignedReviewers = reviewSystem_->getAssignedReviewers(paperId);
    for (uint32_t id : assignedReviewers) {
        if (id == reviewerId) {
            return createJsonResponse(false, "该评审人已分配给此论文", {}, headers);
        }
    }
    
    if (reviewSystem_->assignReviewer(userId, paperId, reviewerId)) {
        std::cout << "✅ 评审分配成功: Paper " << paperId << " -> Reviewer " << reviewerId << std::endl;
        return createJsonResponse(true, "评审分配成功", {}, headers);
    } else {
        return createJsonResponse(false, "评审分配失败", {}, headers);
    }
}

std::string HttpHandler::handleGetReviewProgress(const std::map<std::string, std::string>& headers) {
    std::cout << "📊 获取评审进度" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    User currentUser;
    if (!userManager_->getUserById(userId, currentUser) || currentUser.role != UserRole::EDITOR) {
        return createJsonResponse(false, "权限不足，只有编辑可以查看评审进度", {}, headers);
    }
    
    auto papers = reviewSystem_->getAllPapers();
    std::ostringstream json;
    json << "[";
    
    for (size_t i = 0; i < papers.size(); ++i) {
        if (i > 0) json << ",";
        json << "{"
             << "\"paperId\":" << papers[i].paperId << ","
             << "\"title\":\"" << escapeJson(papers[i].title) << "\","
             << "\"status\":\"" << papers[i].getStatusName() << "\","
             << "\"reviewers\":[";
             
        auto reviewerIds = reviewSystem_->getAssignedReviewers(papers[i].paperId);
        auto reviews = reviewSystem_->getReviewsForPaper(papers[i].paperId);
        
        bool firstReviewer = true;
        for (size_t j = 0; j < reviewerIds.size(); ++j) {
            User reviewer;
            std::string reviewerName = "Unknown";
            bool userExists = userManager_->getUserById(reviewerIds[j], reviewer);
            
            if (userExists) {
                reviewerName = reviewer.username;
            } else {
                // 如果用户不存在，跳过显示，或者显示为 Unknown (ID)
                // 用户要求修复 "Unknown待评审"，意味着不希望看到无效用户
                // 这里我们选择跳过无效用户
                continue;
            }

            if (!firstReviewer) json << ",";
            firstReviewer = false;
            
            // 检查是否已评审
            bool reviewed = false;
            std::string decision = "PENDING";
            for (const auto& r : reviews) {
                if (r.reviewerId == reviewerIds[j]) {
                    reviewed = true;
                    decision = r.getDecisionName();
                    break;
                }
            }
            
            json << "{"
                 << "\"id\":" << reviewerIds[j] << ","
                 << "\"name\":\"" << escapeJson(reviewerName) << "\","
                 << "\"status\":\"" << (reviewed ? "COMPLETED" : "PENDING") << "\","
                 << "\"decision\":\"" << decision << "\""
                 << "}";
        }
        
        json << "]}";
    }
    
    json << "]";
    
    std::map<std::string, std::string> data = {
        {"progress", json.str()}
    };
    
    return createJsonResponse(true, "评审进度获取成功", data, headers);
}

// ============================================================================
// 统计信息
// ============================================================================

std::string HttpHandler::handleGetStatistics(const std::map<std::string, std::string>& headers) {
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
        {"total_reviews", std::to_string(reviewSystem_->getReviewCount())}
    };
    
    return createJsonResponse(true, "统计信息获取成功", data, headers);
}

// ============================================================================
// 新增功能实现
// ============================================================================

std::string HttpHandler::handleUploadRevision(const std::string& paperIdStr, const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "📝 上传论文修订版 - ID: " << paperIdStr << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    uint32_t paperId = std::stoi(paperIdStr);
    std::string fileDataStr = parseJsonField(body, "fileData");
    
    if (fileDataStr.empty()) {
        return createJsonResponse(false, "文件内容不能为空", {}, headers);
    }
    
    std::vector<char> fileData = base64Decode(fileDataStr);
    
    if (reviewSystem_->uploadRevision(paperId, userId, fileData)) {
        return createJsonResponse(true, "修订版上传成功", {}, headers);
    } else {
        return createJsonResponse(false, "修订版上传失败", {}, headers);
    }
}

std::string HttpHandler::handleUpdatePaperFile(const std::string& paperIdStr, const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "📝 更新论文文件 - ID: " << paperIdStr << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    uint32_t paperId = std::stoi(paperIdStr);
    std::string fileDataStr = parseJsonField(body, "fileData");
    
    if (fileDataStr.empty()) {
        return createJsonResponse(false, "文件内容不能为空", {}, headers);
    }
    
    std::vector<char> fileData = base64Decode(fileDataStr);
    
    if (reviewSystem_->updatePaperFile(paperId, userId, fileData)) {
        return createJsonResponse(true, "论文文件更新成功", {}, headers);
    } else {
        return createJsonResponse(false, "更新失败 (状态必须为SUBMITTED)", {}, headers);
    }
}

std::string HttpHandler::handleDownloadPaper(const std::string& paperIdStr, const std::map<std::string, std::string>& headers) {
    std::cout << "📥 下载论文 - ID: " << paperIdStr << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    uint32_t paperId = std::stoi(paperIdStr);
    std::vector<char> data;
    
    if (reviewSystem_->downloadPaper(paperId, data)) {
        std::string content(data.begin(), data.end());
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += createCorsHeaders(headers);
        response += "Content-Type: application/pdf\r\n"; // 假设是PDF
        response += "Content-Disposition: attachment; filename=\"paper_" + paperIdStr + ".pdf\"\r\n";
        response += "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n";
        response += content;
        return response;
    } else {
        return createJsonResponse(false, "下载失败或文件不存在", {}, headers);
    }
}

std::string HttpHandler::handleDownloadReviewAttachment(const std::string& reviewIdStr, const std::map<std::string, std::string>& headers) {
    std::cout << "📥 下载评审附件 - ID: " << reviewIdStr << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    uint32_t reviewId = std::stoi(reviewIdStr);
    Review review = reviewSystem_->getReviewInfo(reviewId);
    
    if (review.reviewId == 0) {
        return createJsonResponse(false, "评审不存在", {}, headers);
    }
    
    // Check permissions: Author of paper, Reviewer (self), or Editor
    bool isAuthor = reviewSystem_->isAuthorOfPaper(userId, review.paperId);
    bool isReviewer = (review.reviewerId == userId);
    bool isEditor = false; 
    
    User user;
    if (userManager_->getUserById(userId, user)) {
        if (user.role == UserRole::EDITOR || user.role == UserRole::ADMIN) {
            isEditor = true;
        }
    }
    
    if (!isAuthor && !isReviewer && !isEditor) {
        return createJsonResponse(false, "权限不足", {}, headers);
    }
    
    // Read file
    std::vector<char> data;
    if (filesystem_->readFile(review.filepath, data)) {
        std::string content(data.begin(), data.end());
        
        // Extract extension from filepath
        std::string extension = "";
        size_t dotPos = review.filepath.find_last_of('.');
        if (dotPos != std::string::npos) {
            extension = review.filepath.substr(dotPos);
        }
        
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += createCorsHeaders(headers);
        response += "Content-Type: application/octet-stream\r\n"; 
        response += "Content-Disposition: attachment; filename=\"review_" + reviewIdStr + "_attachment" + extension + "\"\r\n";
        response += "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n";
        response += content;
        return response;
    } else {
        return createJsonResponse(false, "文件不存在或无法读取", {}, headers);
    }
}

std::string HttpHandler::handleMakeDecision(const std::string& paperIdStr, const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "⚖️ 录用决定 - ID: " << paperIdStr << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    uint32_t paperId = std::stoi(paperIdStr);
    std::string decisionStr = parseJsonField(body, "decision");
    
    PaperStatus status = PaperStatus::SUBMITTED;
    if (decisionStr == "ACCEPTED") status = PaperStatus::ACCEPTED;
    else if (decisionStr == "REJECTED") status = PaperStatus::REJECTED;
    else {
        return createJsonResponse(false, "无效的决定状态", {}, headers);
    }
    
    if (reviewSystem_->makeFinalDecision(userId, paperId, status)) {
        return createJsonResponse(true, "录用决定已保存", {}, headers);
    } else {
        return createJsonResponse(false, "操作失败", {}, headers);
    }
}

std::string HttpHandler::handleUpdateUserRole(const std::string& userIdStr, const std::string& body, const std::map<std::string, std::string>& headers) {
    std::cout << "👤 更新用户角色 - ID: " << userIdStr << std::endl;
    
    uint32_t sessionId, adminId;
    if (!validateAuth(headers, sessionId, adminId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    // 检查管理员权限
    User admin;
    if (!userManager_->getUserById(adminId, admin) || admin.role != UserRole::ADMIN) {
        return createJsonResponse(false, "权限不足", {}, headers);
    }
    
    uint32_t targetUserId = std::stoi(userIdStr);
    std::string roleStr = parseJsonField(body, "role");
    
    UserRole newRole = UserRole::AUTHOR;
    if (roleStr == "REVIEWER") newRole = UserRole::REVIEWER;
    else if (roleStr == "EDITOR") newRole = UserRole::EDITOR;
    else if (roleStr == "ADMIN") newRole = UserRole::ADMIN;
    else if (roleStr != "AUTHOR") {
        return createJsonResponse(false, "无效的角色", {}, headers);
    }
    
    if (userManager_->updateUserRole(targetUserId, newRole)) {
        return createJsonResponse(true, "用户角色更新成功", {}, headers);
    } else {
        return createJsonResponse(false, "更新失败", {}, headers);
    }
}

std::string HttpHandler::handleDeactivateUser(const std::string& userIdStr, const std::map<std::string, std::string>& headers) {
    std::cout << "🚫 停用用户 - ID: " << userIdStr << std::endl;
    
    uint32_t sessionId, adminId;
    if (!validateAuth(headers, sessionId, adminId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    User admin;
    if (!userManager_->getUserById(adminId, admin) || admin.role != UserRole::ADMIN) {
        return createJsonResponse(false, "权限不足", {}, headers);
    }
    
    uint32_t targetUserId = std::stoi(userIdStr);
    
    // Use deleteUser instead of deactivateUser for thorough removal
    if (userManager_->deleteUser(targetUserId)) {
        return createJsonResponse(true, "用户已彻底删除", {}, headers);
    } else {
        return createJsonResponse(false, "操作失败", {}, headers);
    }
}

std::string HttpHandler::handleSystemBackup(const std::map<std::string, std::string>& headers) {
    std::cout << "💾 系统备份" << std::endl;
    
    uint32_t sessionId, adminId;
    if (!validateAuth(headers, sessionId, adminId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    User admin;
    if (!userManager_->getUserById(adminId, admin) || admin.role != UserRole::ADMIN) {
        return createJsonResponse(false, "权限不足", {}, headers);
    }
    
    bool metaSuccess = reviewSystem_->saveMetadata();
    bool userSuccess = userManager_->saveToFile("users.dat");
    
    if (metaSuccess && userSuccess) {
        addSystemLog("INFO", "System backup completed successfully");
        return createJsonResponse(true, "系统备份成功", {}, headers);
    } else {
        addSystemLog("ERROR", "System backup failed");
        return createJsonResponse(false, "备份部分失败", {}, headers);
    }
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

std::string HttpHandler::createJsonResponse(bool success, const std::string& message, const std::map<std::string, std::string>& data, const std::map<std::string, std::string>& headers) {
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
    
    return "HTTP/1.1 200 OK\r\n" + createCorsHeaders(headers) + "Content-Type: application/json\r\n\r\n" + json.str();
}

std::string HttpHandler::createErrorResponse(const std::string& error, const std::map<std::string, std::string>& headers) {
    return std::string("HTTP/1.1 400 Bad Request\r\n") + createCorsHeaders(headers) + 
           "Content-Type: application/json\r\n\r\n" +
           "{\"success\":false,\"message\":\"" + escapeJson(error) + "\"}";
}

std::string HttpHandler::parseJsonField(const std::string& json, const std::string& field) {
    std::string key = "\"" + field + "\"";
    size_t keyPos = json.find(key);
    if (keyPos == std::string::npos) {
        if (field == "fileData") std::cout << "DEBUG: key not found: " << key << std::endl;
        return "";
    }
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) {
        if (field == "fileData") std::cout << "DEBUG: colon not found after key" << std::endl;
        return "";
    }
    
    size_t valueStart = json.find('"', colonPos);
    if (valueStart == std::string::npos) {
        if (field == "fileData") std::cout << "DEBUG: start quote not found after colon" << std::endl;
        return "";
    }
    valueStart++; // Skip quote
    
    // Find end quote, handling escaped quotes
    size_t valueEnd = valueStart;
    while (true) {
        valueEnd = json.find('"', valueEnd);
        if (valueEnd == std::string::npos) {
            if (field == "fileData") std::cout << "DEBUG: end quote not found" << std::endl;
            return "";
        }
        
        // Count backslashes before valueEnd to determine if quote is escaped
        size_t backslashCount = 0;
        size_t checkPos = valueEnd - 1;
        while (checkPos >= valueStart && json[checkPos] == '\\') {
            backslashCount++;
            if (checkPos == 0) break;
            checkPos--;
        }
        
        if (backslashCount % 2 == 0) {
            break; // Even number of backslashes means quote is NOT escaped
        }
        
        valueEnd++; // Skip escaped quote
    }
    
    if (field == "fileData") {
        std::cout << "DEBUG: Found fileData. Start: " << valueStart << ", End: " << valueEnd << ", Length: " << (valueEnd - valueStart) << std::endl;
    }
    
    return json.substr(valueStart, valueEnd - valueStart);
}

std::string HttpHandler::escapeJson(const std::string& str) {
    std::ostringstream ss;
    for (char c : str) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    ss << c;
                }
        }
    }
    return ss.str();
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

// ============================================================================
// 用户管理 API
// ============================================================================

std::string HttpHandler::handleGetUserProfile(const std::map<std::string, std::string>& headers) {
    std::cout << "👤 获取用户资料" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    User user;
    if (userManager_->getUserById(userId, user)) {
        std::map<std::string, std::string> data = {
            {"userId", std::to_string(user.userId)},
            {"username", user.username},
            {"role", user.getRoleName()},
            {"status", user.isActive ? "active" : "inactive"}
        };
        
        return createJsonResponse(true, "用户资料获取成功", data, headers);
    } else {
        return createJsonResponse(false, "用户不存在", {}, headers);
    }
}

std::string HttpHandler::handleGetSystemStats(const std::string& timeRange, const std::map<std::string, std::string>& headers) {
    std::cout << "📊 获取系统统计 - 时间范围: " << timeRange << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    User currentUser;
    if (!userManager_->getUserById(userId, currentUser) || currentUser.role != UserRole::ADMIN) {
        return createJsonResponse(false, "权限不足，只有管理员可以查看系统统计", {}, headers);
    }
    
    auto paperStats = reviewSystem_->getStatistics();
    
    // 计算论文统计数据
    uint32_t total_papers = 0;
    uint32_t submitted_papers = 0;
    uint32_t under_review_papers = 0;
    uint32_t accepted_papers = 0;
    uint32_t rejected_papers = 0;
    
    for (const auto& [status, count] : paperStats) {
        total_papers += count;
        if (status == PaperStatus::SUBMITTED) submitted_papers = count;
        else if (status == PaperStatus::UNDER_REVIEW) under_review_papers = count;
        else if (status == PaperStatus::ACCEPTED) accepted_papers = count;
        else if (status == PaperStatus::REJECTED) rejected_papers = count;
    }
    
    // 计算用户统计数据
    uint32_t total_users = userManager_->getUserCount();
    uint32_t active_users = total_users; // 简化处理
    
    // 获取在线用户数
    auto onlineUsers = userManager_->getOnlineUsers();
    
    // 获取系统资源使用情况 (模拟/简单计算)
    long disk_usage_mb = 0;
    std::ifstream diskFile("disk.img", std::ios::binary | std::ios::ate);
    if (diskFile.is_open()) {
        disk_usage_mb = diskFile.tellg() / (1024 * 1024);
    }
    
    long memory_usage_mb = 0;
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        memory_usage_mb = usage.ru_maxrss / 1024; // Linux returns KB
    }

    std::map<std::string, std::string> data = {
        {"timeRange", timeRange},
        {"total_papers", std::to_string(total_papers)},
        {"submitted_papers", std::to_string(submitted_papers)},
        {"under_review_papers", std::to_string(under_review_papers)},
        {"accepted_papers", std::to_string(accepted_papers)},
        {"rejected_papers", std::to_string(rejected_papers)},
        {"total_users", std::to_string(total_users)},
        {"active_users", std::to_string(active_users)},
        {"online_users", std::to_string(onlineUsers.size())},
        {"disk_usage_mb", std::to_string(disk_usage_mb)},
        {"memory_usage_mb", std::to_string(memory_usage_mb)},
        {"total_reviews", "0"} 
    };
    
    // addSystemLog("INFO", "System stats retrieved by admin"); // Reduced verbosity
    return createJsonResponse(true, "系统统计获取成功", data, headers);
}

std::string HttpHandler::handleGetSystemLogs(const std::map<std::string, std::string>& headers) {
    std::cout << "📜 获取系统日志" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    User currentUser;
    if (!userManager_->getUserById(userId, currentUser) || currentUser.role != UserRole::ADMIN) {
        return createJsonResponse(false, "权限不足", {}, headers);
    }
    
    std::lock_guard<std::mutex> lock(logMutex);
    std::ostringstream logsJson;
    logsJson << "[";
    for (size_t i = 0; i < systemLogs_.size(); ++i) {
        if (i > 0) logsJson << ",";
        logsJson << "\"" << escapeJson(systemLogs_[i]) << "\"";
    }
    logsJson << "]";
    
    std::map<std::string, std::string> data = {
        {"logs", logsJson.str()}
    };
    
    return createJsonResponse(true, "系统日志获取成功", data, headers);
}

std::string HttpHandler::handleGetOnlineUsers(const std::map<std::string, std::string>& headers) {
    std::cout << "👥 获取在线用户列表" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    User currentUser;
    if (!userManager_->getUserById(userId, currentUser) || currentUser.role != UserRole::ADMIN) {
        return createJsonResponse(false, "权限不足，只有管理员可以查看在线用户", {}, headers);
    }
    
    try {
        std::vector<User> onlineUsers;
        uint32_t totalUsers = userManager_->getUserCount();
        
        // 获取所有用户并检查其会话状态
        std::vector<User> allUsers = userManager_->listAllUsers();
        for (const auto& user : allUsers) {
            if (user.isActive) {
                // 这里简化处理，实际中可以检查用户的最后活动时间
                // 为了演示，我们假设所有活跃用户都是在线的
                onlineUsers.push_back(user);
            }
        }
        
        // 构建用户列表JSON
        std::ostringstream usersJson;
        usersJson << "[";
        for (size_t i = 0; i < onlineUsers.size(); ++i) {
            if (i > 0) usersJson << ",";
            usersJson << "{"
                     << "\"userId\":" << onlineUsers[i].userId << ","
                     << "\"username\":\"" << onlineUsers[i].username << "\","
                     << "\"role\":\"" << onlineUsers[i].getRoleName() << "\","
                     << "\"status\":\"在线\""
                     << "}";
        }
        usersJson << "]";
        
        std::map<std::string, std::string> data = {
            {"users", usersJson.str()},
            {"total", std::to_string(onlineUsers.size())}
        };
        
        return createJsonResponse(true, "在线用户列表获取成功", data, headers);
    } catch (const std::exception& e) {
        std::cout << "❌ 获取在线用户列表失败: " << e.what() << std::endl;
        return createErrorResponse("获取在线用户列表失败: " + std::string(e.what()), headers);
    }
}

std::string HttpHandler::handleGetUsers(const std::string& page, const std::string& limit, const std::string& search, const std::string& role, const std::map<std::string, std::string>& headers) {
    std::cout << "👥 获取用户列表" << std::endl;
    
    uint32_t sessionId, userId;
    if (!validateAuth(headers, sessionId, userId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    // 只有编辑和管理员可以查看所有用户
    User currentUser;
    if (!userManager_->getUserById(userId, currentUser) || (currentUser.role != UserRole::EDITOR && currentUser.role != UserRole::ADMIN)) {
        return createJsonResponse(false, "权限不足", {}, headers);
    }
    
    auto allUsers = userManager_->listAllUsers();
    std::vector<User> filteredUsers;
    
    for (const auto& user : allUsers) {
        // 简单的过滤逻辑
        if (!role.empty()) {
            std::string userRole = user.getRoleName();
            std::string userRoleUpper = userRole;
            std::transform(userRoleUpper.begin(), userRoleUpper.end(), userRoleUpper.begin(), ::toupper);
            
            std::string roleUpper = role;
            std::transform(roleUpper.begin(), roleUpper.end(), roleUpper.begin(), ::toupper);
            
            if (userRoleUpper != roleUpper) {
                continue;
            }
        }
        filteredUsers.push_back(user);
    }
    
    std::ostringstream usersJson;
    usersJson << "[";
    for (size_t i = 0; i < filteredUsers.size(); ++i) {
        if (i > 0) usersJson << ",";
        usersJson << "{"
                 << "\"userId\":" << filteredUsers[i].userId << ","
                 << "\"username\":\"" << filteredUsers[i].username << "\","
                 << "\"role\":\"" << filteredUsers[i].getRoleName() << "\","
                 << "\"isActive\":" << (filteredUsers[i].isActive ? "true" : "false")
                 << "}";
    }
    usersJson << "]";
    
    std::map<std::string, std::string> data = {
        {"users", usersJson.str()},
        {"total", std::to_string(filteredUsers.size())}
    };
    
    return createJsonResponse(true, "用户列表获取成功", data, headers);
}

std::string HttpHandler::handleGetOverview(const std::map<std::string, std::string>& headers) {
    std::cout << "📈 获取系统概览" << std::endl;
    
    auto paperStats = reviewSystem_->getStatistics();
    
    // 计算总数据
    uint32_t total_papers = 0;
    for (const auto& [status, count] : paperStats) {
        total_papers += count;
    }
    
    uint32_t total_users = userManager_->getUserCount();
    uint32_t total_reviews = 0; // 这里应该实现实际计算
    
    // 计算最近的论文提交（简化处理）
    uint32_t recent_papers = total_papers; // 假设所有论文都是最近的
    
    std::map<std::string, std::string> data = {
        {"total_papers", std::to_string(total_papers)},
        {"total_users", std::to_string(total_users)},
        {"total_reviews", std::to_string(total_reviews)},
        {"recent_papers", std::to_string(recent_papers)},
        {"system_status", "running"},
        {"last_updated", std::to_string(std::time(nullptr))}
    };
    
    return createJsonResponse(true, "系统概览获取成功", data, headers);
}

// ============================================================================
// CORS 支持
// ============================================================================

std::string HttpHandler::createCorsHeaders(const std::map<std::string, std::string>& headers) {
    // 从请求头中获取Origin
    std::string origin = "*";
    auto it = headers.find("Origin");
    if (it != headers.end()) {
        origin = it->second;
    }
    
    // 构建CORS响应头
    std::string corsHeaders = "Access-Control-Allow-Origin: " + origin + "\r\n";
    
    // 如果有Origin（不是通配符），则允许凭据
    if (origin != "*") {
        corsHeaders += "Access-Control-Allow-Credentials: true\r\n";
    }
    
    corsHeaders += "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                  "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
                  "Access-Control-Max-Age: 86400\r\n";
    
    return corsHeaders;
}

std::string HttpHandler::getMimeType(const std::string& path) {
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos) return "application/octet-stream";
    
    std::string ext = path.substr(dotPos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".html") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".woff") return "font/woff";
    if (ext == ".woff2") return "font/woff2";
    if (ext == ".ttf") return "font/ttf";
    
    return "application/octet-stream";
}

std::string HttpHandler::handleStaticFile(const std::string& path) {
    // 安全检查：防止目录遍历
    if (path.find("..") != std::string::npos) {
        return createErrorResponse("403 Forbidden", {});
    }
    
    std::string webRoot = "../web"; // 假设 server 运行在 build 目录，web 在 ../web
    std::string fullPath = webRoot + path;
    
    // 尝试打开文件
    std::ifstream file(fullPath, std::ios::binary);
    if (!file.is_open()) {
        // 尝试相对于当前目录
        webRoot = "web";
        fullPath = webRoot + path;
        file.open(fullPath, std::ios::binary);
        if (!file.is_open()) {
             // 再试一次绝对路径（假设项目根目录）
            webRoot = "/home/project/peer-review-system/web";
            fullPath = webRoot + path;
            file.open(fullPath, std::ios::binary);
            if (!file.is_open()) {
                std::cout << "❌ 文件未找到: " << fullPath << std::endl;
                return createErrorResponse("404 Not Found", {});
            }
        }
    }
    
    std::cout << "📄 服务静态文件: " << fullPath << std::endl;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    std::string mimeType = getMimeType(path);
    
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << mimeType << "\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "Connection: keep-alive\r\n";
    response << "\r\n";
    response << content;
    
    return response.str();
}

// ============================================================================
// 系统快照管理
// ============================================================================

std::string HttpHandler::handleCreateSnapshot(const std::map<std::string, std::string>& headers) {
    uint32_t sessionId, adminId;
    if (!validateAuth(headers, sessionId, adminId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    User admin;
    if (!userManager_->getUserById(adminId, admin) || admin.role != UserRole::ADMIN) {
        return createJsonResponse(false, "需要管理员权限", {}, headers);
    }
    
    // 1. 确保所有数据写入磁盘
    reviewSystem_->saveMetadata();
    userManager_->saveToFile("users.dat");
    filesystem_->sync();
    
    // 2. 生成快照名称 (时间戳)
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    std::string snapshotName = ss.str();
    
    // 3. 创建备份目录
    namespace fs = std::filesystem;
    std::string backupRootDir = "backups";
    std::string targetDir = backupRootDir + "/" + snapshotName;
    
    try {
        if (!fs::exists(backupRootDir)) {
            fs::create_directory(backupRootDir);
        }
        if (fs::exists(targetDir)) {
            return createJsonResponse(false, "快照已存在 (操作过快?)", {}, headers);
        }
        fs::create_directory(targetDir);
        
        // 4. 复制核心文件
        // 核心文件: 磁盘镜像 (包含文件系统和Review数据), users.dat (用户数据)
        std::string currentDiskPath = filesystem_->getDiskImagePath();
        std::string diskFileName = fs::path(currentDiskPath).filename().string();
        
        if (fs::exists(currentDiskPath)) {
            fs::copy_file(currentDiskPath, targetDir + "/" + diskFileName);
        }
        if (fs::exists("users.dat")) {
            fs::copy_file("users.dat", targetDir + "/users.dat");
        }
        
        // 保存元数据以记录使用了哪个磁盘文件
        std::ofstream metaFile(targetDir + "/snapshot.meta");
        if (metaFile.is_open()) {
            metaFile << "disk_image=" << diskFileName << std::endl;
            metaFile.close();
        }
        
        addSystemLog("INFO", "Created system snapshot: " + snapshotName);
        return createJsonResponse(true, "快照创建成功", {{"snapshotId", snapshotName}}, headers);
        
    } catch (const fs::filesystem_error& e) {
        addSystemLog("ERROR", "Snapshot creation failed: " + std::string(e.what()));
        return createJsonResponse(false, std::string("快照创建失败: ") + e.what(), {}, headers);
    }
}

std::string HttpHandler::handleListSnapshots(const std::map<std::string, std::string>& headers) {
    try {
        uint32_t sessionId, adminId;
        if (!validateAuth(headers, sessionId, adminId)) {
            return createJsonResponse(false, "未认证", {}, headers);
        }
        
        User admin;
        if (!userManager_->getUserById(adminId, admin) || admin.role != UserRole::ADMIN) {
            return createJsonResponse(false, "需要管理员权限", {}, headers);
        }
        
        namespace fs = std::filesystem;
        std::string backupRootDir = "backups";
        std::vector<std::map<std::string, std::string>> snapshots;
        
        if (fs::exists(backupRootDir) && fs::is_directory(backupRootDir)) {
            for (const auto& entry : fs::directory_iterator(backupRootDir)) {
                if (entry.is_directory()) {
                    std::string name = entry.path().filename().string();
                    std::string timeStr;
                    
                    // 尝试解析时间格式 YYYYMMDD_HHMMSS
                    if (name.length() >= 15) { 
                        timeStr = name.substr(0, 4) + "-" + name.substr(4, 2) + "-" + name.substr(6, 2) + " " +
                                 name.substr(9, 2) + ":" + name.substr(11, 2) + ":" + name.substr(13, 2);
                    } else {
                        timeStr = name;
                    }
                    
                    // 获取大小
                    uintmax_t size = 0;
                    try {
                        for(const auto& subEntry : fs::recursive_directory_iterator(entry.path())) {
                            if (!fs::is_directory(subEntry)) {
                                size += fs::file_size(subEntry);
                            }
                        }
                    } catch (...) {}
                    
                    std::stringstream sizeSS;
                    sizeSS << std::fixed << std::setprecision(2) << (double)size / (1024*1024) << " MB";
                    
                    snapshots.push_back({
                        {"id", name},
                        {"name", name},
                        {"created", timeStr},
                        {"size", sizeSS.str()}
                    });
                }
            }
        }
        
        // 按名称倒序排列 (最新在前)
        std::sort(snapshots.begin(), snapshots.end(), [](const auto& a, const auto& b) {
            return a.at("id") > b.at("id");
        });
        
        // 手动构建 JSON 数组字符串
        std::stringstream jsonArray;
        jsonArray << "[";
        for (size_t i = 0; i < snapshots.size(); ++i) {
            jsonArray << "{";
            jsonArray << "\"id\":\"" << escapeJson(snapshots[i]["id"]) << "\",";
            jsonArray << "\"name\":\"" << escapeJson(snapshots[i]["name"]) << "\",";
            jsonArray << "\"timestamp\":\"" << escapeJson(snapshots[i]["created"]) << "\",";
            jsonArray << "\"size\":\"" << escapeJson(snapshots[i]["size"]) << "\"";
            jsonArray << "}";
            if (i < snapshots.size() - 1) jsonArray << ",";
        }
        jsonArray << "]";
        
        // 构建完整 JSON 响应对象
        std::stringstream fullJson;
        fullJson << "{";
        fullJson << "\"success\":true,";
        fullJson << "\"message\":\"OK\",";
        fullJson << "\"data\":{ \"snapshots\": " << jsonArray.str() << " }";
        fullJson << "}";
        
        // 构建完整 HTTP 响应
        std::ostringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Content-Length: " << fullJson.str().length() << "\r\n";
        response << createCorsHeaders(headers); // 包含了结尾的 \r\n
        response << "\r\n"; // 空行分割 Header 和 Body
        response << fullJson.str();
        
        return response.str();
    } catch (const std::exception& e) {
        addSystemLog("ERROR", "List snapshots failed: " + std::string(e.what()));
        return createJsonResponse(false, "获取快照列表失败", {}, headers);
    }
}

std::string HttpHandler::handleRestoreSnapshot(const std::string& body, const std::map<std::string, std::string>& headers) {
    uint32_t sessionId, adminId;
    if (!validateAuth(headers, sessionId, adminId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    User admin;
    if (!userManager_->getUserById(adminId, admin) || admin.role != UserRole::ADMIN) {
        return createJsonResponse(false, "需要管理员权限", {}, headers);
    }
    
    std::string snapshotId = parseJsonField(body, "snapshotId");
    if (snapshotId.empty()) {
        return createJsonResponse(false, "缺少 snapshotId", {}, headers);
    }
    
    namespace fs = std::filesystem;
    std::string backupRootDir = "backups";
    std::string sourceDir = backupRootDir + "/" + snapshotId;
    
    if (!fs::exists(sourceDir)) {
        return createJsonResponse(false, "快照不存在", {}, headers);
    }
    
    try {
        // 1. 先卸载文件系统，释放文件占用并防止旧数据回写
        filesystem_->unmount();
        
        // 2. 覆盖文件
        std::string currentDiskPath = filesystem_->getDiskImagePath();
        
        // 确定备份中的磁盘文件路径
        namespace fs = std::filesystem;
        std::string backupDiskPath = sourceDir + "/disk.img"; // 默认/旧版兼容
        
        if (fs::exists(sourceDir + "/snapshot.meta")) {
            std::ifstream meta(sourceDir + "/snapshot.meta");
            std::string line;
            while(std::getline(meta, line)) {
                if (line.find("disk_image=") == 0) {
                    std::string val = line.substr(11);
                    // remove \r if present
                    if (!val.empty() && val.back() == '\r') val.pop_back();
                    backupDiskPath = sourceDir + "/" + val;
                    break;
                }
            }
        } else {
             // 尝试使用当前磁盘名称
             std::string currentName = fs::path(currentDiskPath).filename().string();
             if (!fs::exists(backupDiskPath) && fs::exists(sourceDir + "/" + currentName)) {
                 backupDiskPath = sourceDir + "/" + currentName;
             }
        }
        
        if (fs::exists(backupDiskPath)) {
            std::cout << "Restoring disk from " << backupDiskPath << " to " << currentDiskPath << std::endl;
            fs::copy_file(backupDiskPath, currentDiskPath, fs::copy_options::overwrite_existing);
        } else {
            addSystemLog("WARN", "Disk image not found in snapshot: " + snapshotId);
        }

        if (fs::exists(sourceDir + "/users.dat")) {
            try {
                // Remove existing file first to avoid permission issues with overwrite
                if (fs::exists("users.dat")) {
                    fs::remove("users.dat");
                }
                fs::copy_file(sourceDir + "/users.dat", "users.dat");
            } catch (const std::exception& e) {
                 addSystemLog("ERROR", "Failed to restore users.dat: " + std::string(e.what()));
                 // Fallback to manual copy if fs::copy_file fails
                 std::ifstream src(sourceDir + "/users.dat", std::ios::binary);
                 std::ofstream dst("users.dat", std::ios::binary);
                 dst << src.rdbuf();
            }
        }
        
        // 3. 重新加载系统组件
        if (!filesystem_->mount()) {
            addSystemLog("CRITICAL", "Failed to remount filesystem after restore!");
            return createJsonResponse(false, "还原后挂载文件系统失败，系统可能处于不稳定状态", {}, headers);
        }
        
        // UserManager: 重新加载用户
        userManager_->loadFromFile("users.dat");
        
        // ReviewSystem: 重新初始化
        reviewSystem_->init();
        
        addSystemLog("INFO", "Restored system from snapshot: " + snapshotId);
        return createJsonResponse(true, "系统已还原", {}, headers);
        
    } catch (const std::exception& e) {
        addSystemLog("ERROR", "Restore failed: " + std::string(e.what()));
        return createJsonResponse(false, std::string("还原失败: ") + e.what(), {}, headers);
    }
}

std::string HttpHandler::handleDeleteSnapshot(const std::string& body, const std::map<std::string, std::string>& headers) {
    uint32_t sessionId, adminId;
    if (!validateAuth(headers, sessionId, adminId)) {
        return createJsonResponse(false, "未认证", {}, headers);
    }
    
    User admin;
    if (!userManager_->getUserById(adminId, admin) || admin.role != UserRole::ADMIN) {
        return createJsonResponse(false, "需要管理员权限", {}, headers);
    }
    
    std::string snapshotId = parseJsonField(body, "snapshotId");
    if (snapshotId.empty()) {
        return createJsonResponse(false, "缺少 snapshotId", {}, headers);
    }
    
    namespace fs = std::filesystem;
    std::string targetDir = "backups/" + snapshotId;
    
    try {
        if (fs::exists(targetDir)) {
            fs::remove_all(targetDir);
            addSystemLog("INFO", "Deleted snapshot: " + snapshotId);
            return createJsonResponse(true, "快照已删除", {}, headers);
        } else {
            return createJsonResponse(false, "快照不存在", {}, headers);
        }
    } catch (const std::exception& e) {
        return createJsonResponse(false, std::string("删除失败: ") + e.what(), {}, headers);
    }
}