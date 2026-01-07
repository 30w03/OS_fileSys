#pragma once
#include <string>
#include <map>
#include <memory>
#include <vector>

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
    std::string handleRegister(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleLogin(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleHeartbeat(const std::map<std::string, std::string>& headers);
    std::string handleLogout(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleGetFiles(const std::map<std::string, std::string>& headers);
    std::string handleUploadFile(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleDeleteFile(const std::string& filename, const std::map<std::string, std::string>& headers);
    std::string handleGetPapers(const std::map<std::string, std::string>& headers);
    std::string handleGetMyPapers(const std::map<std::string, std::string>& headers);
    std::string handleGetPapersToReview(const std::map<std::string, std::string>& headers);
    std::string handleSubmitPaper(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleSubmitReview(const std::string& paperIdStr, const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleAssignReviewer(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleGetStatistics(const std::map<std::string, std::string>& headers);
    std::string handleGetUserProfile(const std::map<std::string, std::string>& headers);
    std::string handleGetSystemStats(const std::string& timeRange, const std::map<std::string, std::string>& headers);
    std::string handleGetSystemLogs(const std::map<std::string, std::string>& headers);
    std::string handleGetOnlineUsers(const std::map<std::string, std::string>& headers);
    std::string handleGetUsers(const std::string& page, const std::string& limit, const std::string& search, const std::string& role, const std::map<std::string, std::string>& headers);
    std::string handleGetReviewProgress(const std::map<std::string, std::string>& headers);
    std::string handleGetOverview(const std::map<std::string, std::string>& headers);
    
    // 新增功能路由
    std::string handleUploadRevision(const std::string& paperIdStr, const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleUpdatePaperFile(const std::string& paperIdStr, const std::string& body, const std::map<std::string, std::string>& headers); // 🔥 New
    std::string handleDownloadPaper(const std::string& paperIdStr, const std::map<std::string, std::string>& headers);
    std::string handleDownloadReviewAttachment(const std::string& reviewIdStr, const std::map<std::string, std::string>& headers);
    std::string handleMakeDecision(const std::string& paperIdStr, const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleUpdateUserRole(const std::string& userIdStr, const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleDeactivateUser(const std::string& userIdStr, const std::map<std::string, std::string>& headers);
    std::string handleSystemBackup(const std::map<std::string, std::string>& headers);

    // Snapshot Management
    std::string handleCreateSnapshot(const std::map<std::string, std::string>& headers);
    std::string handleListSnapshots(const std::map<std::string, std::string>& headers);
    std::string handleRestoreSnapshot(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleDeleteSnapshot(const std::string& body, const std::map<std::string, std::string>& headers);

    std::string handleStaticFile(const std::string& path);
    std::string getMimeType(const std::string& path);
    std::string createHtmlResponse();
    
    // 日志缓冲
    static std::vector<std::string> systemLogs_;
    static void addSystemLog(const std::string& level, const std::string& message);
            
            // 辅助函数
            std::string createJsonResponse(bool success, const std::string& message, 
                                          const std::map<std::string, std::string>& data = {},
                                          const std::map<std::string, std::string>& headers = {});
            std::string createErrorResponse(const std::string& error, const std::map<std::string, std::string>& headers = {});
            std::string parseJsonField(const std::string& json, const std::string& field);
            std::string escapeJson(const std::string& str);
            bool validateAuth(const std::map<std::string, std::string>& headers, uint32_t& sessionId, uint32_t& userId);
            std::string createCorsHeaders(const std::map<std::string, std::string>& headers);
};