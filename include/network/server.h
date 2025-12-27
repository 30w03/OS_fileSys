#pragma once

#include "network/connection.h"
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
    
    // 消息处理函数
    void handleLogin(Connection* client, const Protocol::Message& request);
    void handleRegister(Connection* client, const Protocol::Message& request);
    void handleLogout(Connection* client, const Protocol::Message& request);
    
    void handleSubmitPaper(Connection* client, const Protocol::Message& request);
    void handleGetMyPapers(Connection* client, const Protocol::Message& request);
    void handleGetPapersToReview(Connection* client, const Protocol::Message& request);
    void handleSubmitReview(Connection* client, const Protocol::Message& request);
    void handleAssignReviewer(Connection* client, const Protocol::Message& request);
    void handleGetAllPapers(Connection* client, const Protocol::Message& request);
    void handleDownloadPaper(Connection* client, const Protocol::Message& request);
    void handleGetReviews(Connection* client, const Protocol::Message& request);
    void handleMakeDecision(Connection* client, const Protocol::Message& request);
    void handleGetStatistics(Connection* client, const Protocol::Message& request);
    
    uint16_t port_;
    std::string diskImage_;
    std::shared_ptr<Filesystem> filesystem_;
    std::shared_ptr<UserManager> userManager_;
    std::shared_ptr<ReviewSystem> reviewSystem_;
    
    int serverSocket_;
    std::atomic<bool> running_;
    std::vector<std::thread> clientThreads_;
    std::thread acceptThread_;
};
