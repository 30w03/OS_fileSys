#pragma once
#include <memory>
#include <string>
#include "user/user_manager.h"
#include "review/review_system.h"
#include "filesystem/filesystem.h"

class CLI {
public:
    CLI(std::shared_ptr<UserManager> userMgr, 
        std::shared_ptr<ReviewSystem> reviewSys,
        std::shared_ptr<Filesystem> fs);
    
    void run();
    
private:
    void printWelcome();
    void printHelp();
    void printPrompt();
    
    // 命令处理
    void handleLogin();
    void handleRegister();
    void handleLogout();
    void handleSubmitPaper();
    void handleMyPapers();
    void handleReview();
    void handleAssignReviewer();
    void handleAllPapers();
    void handleStats();
    
    std::shared_ptr<UserManager> userManager_;
    std::shared_ptr<ReviewSystem> reviewSystem_;
    std::shared_ptr<Filesystem> filesystem_;
    User* currentUser_;
    bool running_;
};
