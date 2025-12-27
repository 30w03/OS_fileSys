#include "client/cli.h"
#include "review/review.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

CLI::CLI(std::shared_ptr<UserManager> userMgr,
         std::shared_ptr<ReviewSystem> reviewSys,
         std::shared_ptr<Filesystem> fs)
    : userManager_(userMgr)
    , reviewSystem_(reviewSys)
    , filesystem_(fs)
    , currentUser_(nullptr)
    , running_(false) {}

void CLI::run() {
    running_ = true;
    printWelcome();
    
    while (running_) {
        printPrompt();
        
        std::string line;
        if (!std::getline(std::cin, line)) {
            break;
        }
        
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        
        if (command.empty()) continue;
        
        if (command == "help") {
            printHelp();
        } else if (command == "register") {
            handleRegister();
        } else if (command == "login") {
            handleLogin();
        } else if (command == "logout") {
            handleLogout();
        } else if (command == "submit") {
            handleSubmitPaper();
        } else if (command == "mypapers") {
            handleMyPapers();
        } else if (command == "review") {
            handleReview();
        } else if (command == "assign") {
            handleAssignReviewer();
        } else if (command == "papers") {
            handleAllPapers();
        } else if (command == "stats") {
            handleStats();
        } else if (command == "quit" || command == "exit") {
            running_ = false;
        } else {
            std::cout << "❌ Unknown command. Type 'help' for available commands." << std::endl;
        }
    }
    
    std::cout << "\n👋 Goodbye!\n" << std::endl;
}

void CLI::printWelcome() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                        ║\n";
    std::cout << "║        📚 PEER REVIEW MANAGEMENT SYSTEM 📚            ║\n";
    std::cout << "║                                                        ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Type 'help' to see available commands.\n" << std::endl;
}

void CLI::printHelp() {
    std::cout << "\n📖 Available Commands:\n" << std::endl;
    std::cout << "  Authentication:" << std::endl;
    std::cout << "    register       - Register a new user" << std::endl;
    std::cout << "    login          - Login to system" << std::endl;
    std::cout << "    logout         - Logout from system" << std::endl;
    std::cout << "\n  Author Commands:" << std::endl;
    std::cout << "    submit         - Submit a new paper" << std::endl;
    std::cout << "    mypapers       - View your submitted papers" << std::endl;
    std::cout << "\n  Reviewer Commands:" << std::endl;
    std::cout << "    review         - Review assigned papers" << std::endl;
    std::cout << "\n  Editor Commands:" << std::endl;
    std::cout << "    papers         - View all papers" << std::endl;
    std::cout << "    assign         - Assign reviewer to paper" << std::endl;
    std::cout << "    stats          - View system statistics" << std::endl;
    std::cout << "\n  System:" << std::endl;
    std::cout << "    help           - Show this help message" << std::endl;
    std::cout << "    quit/exit      - Exit the system\n" << std::endl;
}

void CLI::printPrompt() {
    if (currentUser_) {
        std::cout << "[" << currentUser_->username << "] > ";
    } else {
        std::cout << "[guest] > ";
    }
}

// ✅ 使用实际的 createUser 方法
void CLI::handleRegister() {
    std::string username, password, roleStr;
    
    std::cout << "Username: ";
    std::getline(std::cin, username);
    
    std::cout << "Password: ";
    std::getline(std::cin, password);
    
    std::cout << "Role (author/reviewer/editor): ";
    std::getline(std::cin, roleStr);
    
    UserRole role;
    if (roleStr == "author") {
        role = UserRole::AUTHOR;
    } else if (roleStr == "reviewer") {
        role = UserRole::REVIEWER;
    } else if (roleStr == "editor") {
        role = UserRole::EDITOR;
    } else {
        std::cout << "❌ Invalid role" << std::endl;
        return;
    }
    
    // ✅ 使用 createUser 而不是 registerUser
    if (userManager_->createUser(username, password, role)) {
        std::cout << "✅ Registration successful!" << std::endl;
    } else {
        std::cout << "❌ Registration failed (username may already exist)" << std::endl;
    }
}

// ✅ 使用实际的 authenticateUser 和 createSession 方法
void CLI::handleLogin() {
    if (currentUser_) {
        std::cout << "❌ Already logged in as " << currentUser_->username << std::endl;
        return;
    }
    
    std::string username, password;
    
    std::cout << "Username: ";
    std::getline(std::cin, username);
    
    std::cout << "Password: ";
    std::getline(std::cin, password);
    
    uint32_t userId;
    // ✅ 使用 authenticateUser
    if (userManager_->authenticateUser(username, password, userId)) {
        User user;
        if (userManager_->getUserById(userId, user)) {
            currentUser_ = new User(user);
            
            // 创建会话
            uint32_t sessionId = userManager_->createSession(userId);
            
            std::cout << "✅ Welcome, " << username << "!" << std::endl;
            std::cout << "   Role: " << user.getRoleName() << std::endl;
            std::cout << "   Session ID: " << sessionId << std::endl;
        }
    } else {
        std::cout << "❌ Login failed (invalid username or password)" << std::endl;
    }
}

void CLI::handleLogout() {
    if (!currentUser_) {
        std::cout << "❌ Not logged in" << std::endl;
        return;
    }
    
    delete currentUser_;
    currentUser_ = nullptr;
    std::cout << "✅ Logged out successfully" << std::endl;
}

void CLI::handleSubmitPaper() {
    if (!currentUser_) {
        std::cout << "❌ Please login first" << std::endl;
        return;
    }
    
    std::string title, abstract, filepath;
    
    std::cout << "Paper Title: ";
    std::getline(std::cin, title);
    
    std::cout << "Abstract: ";
    std::getline(std::cin, abstract);
    
    std::cout << "File Path: ";
    std::getline(std::cin, filepath);
    
    // 读取文件
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cout << "❌ Cannot open file: " << filepath << std::endl;
        return;
    }
    
    std::vector<char> data((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    file.close();
    
    uint32_t paperId = reviewSystem_->submitPaper(currentUser_->userId, title, abstract, data);
    
    if (paperId > 0) {
        std::cout << "✅ Paper submitted! Paper ID: " << paperId << std::endl;
    } else {
        std::cout << "❌ Failed to submit paper" << std::endl;
    }
}

// ============================================================================
// 🔧 修复问题3：显示详细的评审信息
// ============================================================================
void CLI::handleMyPapers() {
    if (!currentUser_) {
        std::cout << "❌ Please login first" << std::endl;
        return;
    }
    
    auto papers = reviewSystem_->getPapersByAuthor(currentUser_->userId);
    
    if (papers.empty()) {
        std::cout << "📚 You have no submitted papers" << std::endl;
        return;
    }
    
    std::cout << "\n📚 Your Papers (" << papers.size() << "):\n" << std::endl;
    
    for (const auto& paper : papers) {
        std::cout << "┌─────────────────────────────────────────" << std::endl;
        std::cout << "│ Paper ID: " << paper.paperId << std::endl;
        std::cout << "│ Title: " << paper.title << std::endl;
        std::cout << "│ Status: " << paper.getStatusName() << std::endl;
        std::cout << "│ Version: " << paper.currentVersion << std::endl;
        
        // ✅ 显示评审信息
        auto reviews = reviewSystem_->getReviewsForPaper(paper.paperId);
        
        if (!reviews.empty()) {
            std::cout << "│" << std::endl;
            std::cout << "│ 📝 Reviews (" << reviews.size() << "):" << std::endl;
            
            for (const auto& review : reviews) {
                User reviewer;
                if (userManager_->getUserById(review.reviewerId, reviewer)) {
                    std::cout << "│   ";
                    
                    // 决定图标
                    if (review.decision == ReviewDecision::STRONG_ACCEPT ||
                        review.decision == ReviewDecision::ACCEPT ||
                        review.decision == ReviewDecision::WEAK_ACCEPT) {
                        std::cout << "✅ ";
                    } else if (review.decision == ReviewDecision::STRONG_REJECT ||
                               review.decision == ReviewDecision::REJECT ||
                               review.decision == ReviewDecision::WEAK_REJECT) {
                        std::cout << "❌ ";
                    } else {
                        std::cout << "⚠️  ";
                    }
                    
                    std::cout << reviewer.username << ": " 
                              << review.getDecisionName() 
                              << " (Confidence: " << review.confidenceScore << "/5)" 
                              << std::endl;
                    
                    // 显示评论（截取前100字符）
                    std::string comment = review.comments;
                    if (comment.length() > 100) {
                        comment = comment.substr(0, 97) + "...";
                    }
                    std::cout << "│      \"" << comment << "\"" << std::endl;
                }
            }
        } else if (paper.assignedReviewers.size() > 0) {
            std::cout << "│" << std::endl;
            std::cout << "│ ⏳ Waiting for " << paper.assignedReviewers.size() 
                      << " reviewer(s)" << std::endl;
        }
        
        std::cout << "└─────────────────────────────────────────\n" << std::endl;
    }
}

void CLI::handleReview() {
    if (!currentUser_) {
        std::cout << "❌ Please login first" << std::endl;
        return;
    }
    
    auto papers = reviewSystem_->getPapersToReview(currentUser_->userId);
    
    if (papers.empty()) {
        std::cout << "📭 No papers to review" << std::endl;
        return;
    }
    
    std::cout << "\n📝 Papers to Review:\n" << std::endl;
    
    for (const auto& paper : papers) {
        std::cout << "  [" << paper.paperId << "] " << paper.title << std::endl;
    }
    
    std::cout << "\nEnter Paper ID to review (or 0 to cancel): ";
    std::string input;
    std::getline(std::cin, input);
    
    uint32_t paperId = std::stoi(input);
    if (paperId == 0) return;
    
    std::cout << "\nDecision (strong_accept/accept/weak_accept/borderline/weak_reject/reject/strong_reject): ";
    std::string decisionStr;
    std::getline(std::cin, decisionStr);
    
    ReviewDecision decision;
    if (decisionStr == "strong_accept") decision = ReviewDecision::STRONG_ACCEPT;
    else if (decisionStr == "accept") decision = ReviewDecision::ACCEPT;
    else if (decisionStr == "weak_accept") decision = ReviewDecision::WEAK_ACCEPT;
    else if (decisionStr == "borderline") decision = ReviewDecision::BORDERLINE;
    else if (decisionStr == "weak_reject") decision = ReviewDecision::WEAK_REJECT;
    else if (decisionStr == "reject") decision = ReviewDecision::REJECT;
    else if (decisionStr == "strong_reject") decision = ReviewDecision::STRONG_REJECT;
    else {
        std::cout << "❌ Invalid decision" << std::endl;
        return;
    }
    
    std::cout << "Confidence (1-5): ";
    std::getline(std::cin, input);
    int confidence = std::stoi(input);
    
    std::cout << "Comments: ";
    std::string comments;
    std::getline(std::cin, comments);
    
    uint32_t reviewId = reviewSystem_->submitReview(currentUser_->userId, paperId, decision, confidence, comments);
    
    if (reviewId > 0) {
        std::cout << "✅ Review submitted! Review ID: " << reviewId << std::endl;
    } else {
        std::cout << "❌ Failed to submit review" << std::endl;
    }
}

void CLI::handleAssignReviewer() {
    if (!currentUser_) {
        std::cout << "❌ Please login first" << std::endl;
        return;
    }
    
    if (currentUser_->role != UserRole::EDITOR && currentUser_->role != UserRole::ADMIN) {
        std::cout << "❌ Only editors can assign reviewers" << std::endl;
        return;
    }
    
    std::string input;
    
    std::cout << "Paper ID: ";
    std::getline(std::cin, input);
    uint32_t paperId = std::stoi(input);
    
    std::cout << "Reviewer ID: ";
    std::getline(std::cin, input);
    uint32_t reviewerId = std::stoi(input);
    
    if (reviewSystem_->assignReviewer(currentUser_->userId, paperId, reviewerId)) {
        std::cout << "✅ Reviewer assigned successfully" << std::endl;
    } else {
        std::cout << "❌ Failed to assign reviewer" << std::endl;
    }
}

void CLI::handleAllPapers() {
    if (!currentUser_) {
        std::cout << "❌ Please login first" << std::endl;
        return;
    }
    
    auto papers = reviewSystem_->getAllPapers();
    
    if (papers.empty()) {
        std::cout << "📭 No papers in system" << std::endl;
        return;
    }
    
    std::cout << "\n📚 All Papers (" << papers.size() << "):\n" << std::endl;
    
    for (const auto& paper : papers) {
        std::cout << "  [" << paper.paperId << "] " 
                  << paper.title << " - " 
                  << paper.getStatusName() << std::endl;
    }
    std::cout << std::endl;
}

void CLI::handleStats() {
    reviewSystem_->printStatistics();
}
