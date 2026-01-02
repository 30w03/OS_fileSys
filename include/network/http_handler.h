#pragma once
#include <string>
#include <map>
#include <memory>

class UserManager;
class ReviewSystem;
class Filesystem;

class HttpHandler {
public:
    HttpHandler(
        std::shared_ptr<UserManager> userManager,
        std::shared_ptr<ReviewSystem> reviewSystem,
        std::shared_ptr<Filesystem> filesystem
    );
    
    ~HttpHandler() = default;
    
    std::string handleRequest(const std::string& method, const std::string& path, 
                             const std::string& body, const std::map<std::string, std::string>& headers);
    
private:
    std::shared_ptr<UserManager> userManager_;
    std::shared_ptr<ReviewSystem> reviewSystem_;
    std::shared_ptr<Filesystem> filesystem_;
    
    // 路由处理函数
    std::string handleRegister(const std::string& body);
    std::string handleLogin(const std::string& body);
    std::string handleLogout(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleGetFiles(const std::map<std::string, std::string>& headers);
    std::string handleUploadFile(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleDeleteFile(const std::string& filename, const std::map<std::string, std::string>& headers);
    std::string handleGetPapers(const std::map<std::string, std::string>& headers);
    std::string handleGetMyPapers(const std::map<std::string, std::string>& headers);
    std::string handleGetPapersToReview(const std::map<std::string, std::string>& headers);
    std::string handleSubmitPaper(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleSubmitReview(const std::string& paperIdStr, const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleGetStatistics();
    std::string createHtmlResponse();
    
    // 辅助函数
    std::string createJsonResponse(bool success, const std::string& message, 
                                  const std::map<std::string, std::string>& data = {});
    std::string createErrorResponse(const std::string& error);
    std::string parseJsonField(const std::string& json, const std::string& field);
    std::string escapeJson(const std::string& str);
    bool validateAuth(const std::map<std::string, std::string>& headers, uint32_t& sessionId, uint32_t& userId);
};