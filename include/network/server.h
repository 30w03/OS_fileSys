#pragma once

#include "network/connection.h"
#include "network/http_handler.h"
#include "filesystem/filesystem.h"
#include "user/user_manager.h"
#include "review/review_system.h"
#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>

class Server {
public:
    Server(uint16_t port, const std::string& diskImage);
    ~Server();
    
    bool start();
    void stop();
    void run();
    
private:
    void handleClient(std::unique_ptr<Connection> client);
    void acceptLoop();
    
    // HTTP请求处理
    bool isHttpRequest(Connection* client);
    void handleHttpRequest(std::unique_ptr<Connection> client);
    
    // 消息处理函数
    void handleLogin(Connection* client, const Protocol::Message& request);
    void handleRegister(Connection* client, const Protocol::Message& request);
    void handleLogout(Connection* client, const Protocol::Message& request);
    
    void handleSubmitPaper(Connection* client, const Protocol::Message& request);
    void handleAutoAssign(Connection* client, const Protocol::Message& request);
    void handleUpdateProfile(Connection* client, const Protocol::Message& request);
    void handleGetMyPapers(Connection* client, const Protocol::Message& request);
    void handleGetPapersToReview(Connection* client, const Protocol::Message& request);
    void handleSubmitReview(Connection* client, const Protocol::Message& request);
    void handleAssignReviewer(Connection* client, const Protocol::Message& request);
    void handleGetAllPapers(Connection* client, const Protocol::Message& request);
    void handleDownloadPaper(Connection* client, const Protocol::Message& request);
    void handleGetReviews(Connection* client, const Protocol::Message& request);
    void handleMakeDecision(Connection* client, const Protocol::Message& request);
    void handleGetStatistics(Connection* client, const Protocol::Message& request);
    void handleGetSystemStats(Connection* client, const Protocol::Message& request);
    void handleListOnlineUsers(Connection* client, const Protocol::Message& request);
    
    // 监控数据结构
    struct SystemStats {
        uint64_t uptime_seconds;
        uint32_t total_connections;
        uint32_t active_connections;
        uint32_t total_requests;
        uint32_t cache_hit_rate;
        uint32_t memory_usage_mb;
        uint32_t disk_usage_mb;
    };
    
    uint16_t port_;
    std::string diskImage_;
    std::shared_ptr<Filesystem> filesystem_;
    std::shared_ptr<UserManager> userManager_;
    std::shared_ptr<ReviewSystem> reviewSystem_;
    std::shared_ptr<HttpHandler> httpHandler_;
    
    int serverSocket_;
    std::atomic<bool> running_;
    std::vector<std::thread> clientThreads_;
    std::thread acceptThread_;
    
    // 监控数据
    std::atomic<uint64_t> startTime_;
    std::atomic<uint32_t> totalConnections_;
    std::atomic<uint32_t> totalRequests_;
    std::mutex onlineUsersMutex_;
    std::map<uint32_t, std::string> onlineUsers_;  // sessionId -> username
};
