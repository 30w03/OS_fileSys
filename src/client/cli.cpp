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
        } else if (command == "autoassign") {
            handleAutoAssign();
        } else if (command == "profile") {
            handleUpdateProfile();
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
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                                              ║\n";
    std::cout << "║              📚 PEER REVIEW MANAGEMENT SYSTEM 📚                             ║\n";
    std::cout << "║                                                                              ║\n";
    std::cout << "║     A collaborative platform for academic paper submission and review       ║\n";
    std::cout << "║                                                                              ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Welcome! Please type 'help' to see available commands.\n" << std::endl;
}

void CLI::printHelp() {
    std::cout << "\n📖 Available Commands:\n" << std::endl;
    
    std::cout << "  🔐 Authentication:" << std::endl;
    std::cout << "    register       - Register a new user account" << std::endl;
    std::cout << "    login          - Login to the system" << std::endl;
    std::cout << "    logout         - Logout from the system" << std::endl;
    std::cout << "    profile        - Update your profile (institution, interests)" << std::endl;
    std::cout << "\n  📝 Author Commands:" << std::endl;
    std::cout << "    submit         - Submit a new paper for review" << std::endl;
    std::cout << "    mypapers       - View your submitted papers and their status" << std::endl;
    std::cout << "\n  👀 Reviewer Commands:" << std::endl;
    std::cout << "    review         - Review papers assigned to you" << std::endl;
    std::cout << "\n  📊 Editor Commands:" << std::endl;
    std::cout << "    papers         - View all papers in the system" << std::endl;
    std::cout << "    assign         - Assign reviewers to papers" << std::endl;
    std::cout << "    autoassign     - Automatically assign reviewers to a paper" << std::endl;
    std::cout << "    stats          - View system statistics and online users" << std::endl;
    std::cout << "\n  💾 File Operations:" << std::endl;
    std::cout << "    upload         - Upload a file to the server" << std::endl;
    std::cout << "    download       - Download a file from the server" << std::endl;
    std::cout << "    files          - List files on the server" << std::endl;
    std::cout << "    connect        - Connect to the server" << std::endl;
    std::cout << "    disconnect     - Disconnect from the server" << std::endl;
    std::cout << "\n  ℹ️ System:" << std::endl;
    std::cout << "    help           - Show this help message" << std::endl;
    std::cout << "    quit/exit      - Exit the system\n" << std::endl;
    
    std::cout << "Tip: You need to connect to the server and login before you can use most commands.\n" << std::endl;
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
    
    std::string keywordsStr;
    std::cout << "Keywords (comma separated): ";
    std::getline(std::cin, keywordsStr);
    
    std::vector<std::string> keywords;
    std::stringstream ss(keywordsStr);
    std::string kw;
    while (std::getline(ss, kw, ',')) {
        // Trim whitespace
        kw.erase(0, kw.find_first_not_of(" \t"));
        kw.erase(kw.find_last_not_of(" \t") + 1);
        if (!kw.empty()) {
            keywords.push_back(kw);
        }
    }

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
    
    uint32_t paperId = reviewSystem_->submitPaper(currentUser_->userId, title, abstract, data, keywords);
    
    if (paperId > 0) {
        std::cout << "✅ Paper submitted! Paper ID: " << paperId << std::endl;
    } else {
        std::cout << "❌ Failed to submit paper" << std::endl;
    }
}

void CLI::handleUpdateProfile() {
    if (!currentUser_) {
        std::cout << "❌ Please login first" << std::endl;
        return;
    }
    
    std::string institution;
    std::cout << "Institution: ";
    std::getline(std::cin, institution);
    
    std::string interestsStr;
    std::cout << "Research Interests (comma separated): ";
    std::getline(std::cin, interestsStr);
    
    std::vector<std::string> interests;
    std::stringstream ss(interestsStr);
    std::string interest;
    while (std::getline(ss, interest, ',')) {
        // Trim whitespace
        interest.erase(0, interest.find_first_not_of(" \t"));
        interest.erase(interest.find_last_not_of(" \t") + 1);
        if (!interest.empty()) {
            interests.push_back(interest);
        }
    }
    
    int maxLoad;
    std::cout << "Max Review Load (default 3): ";
    std::string loadStr;
    std::getline(std::cin, loadStr);
    if (loadStr.empty()) {
        maxLoad = 3;
    } else {
        try {
            maxLoad = std::stoi(loadStr);
        } catch (...) {
            maxLoad = 3;
        }
    }
    
    if (userManager_->updateUserProfile(currentUser_->userId, institution, interests, maxLoad)) {
        std::cout << "✅ Profile updated successfully!" << std::endl;
        // Update local cache
        currentUser_->institution = institution;
        currentUser_->researchInterests = interests;
        currentUser_->maxLoad = maxLoad;
    } else {
        std::cout << "❌ Failed to update profile" << std::endl;
    }
}

void CLI::handleAutoAssign() {
    if (!currentUser_) {
        std::cout << "❌ Please login first" << std::endl;
        return;
    }
    
    if (currentUser_->role != UserRole::ADMIN && currentUser_->role != UserRole::EDITOR) {
        std::cout << "❌ Permission denied. Only Editors and Admins can assign reviewers." << std::endl;
        return;
    }
    
    std::string paperIdStr;
    std::cout << "Paper ID to auto-assign: ";
    std::getline(std::cin, paperIdStr);
    
    uint32_t paperId;
    try {
        paperId = std::stoi(paperIdStr);
    } catch (...) {
        std::cout << "❌ Invalid Paper ID" << std::endl;
        return;
    }
    
    if (reviewSystem_->autoAssignReviewers(paperId)) {
        std::cout << "✅ Auto-assignment completed successfully!" << std::endl;
        
        // Show assigned reviewers
        auto reviewers = reviewSystem_->getAssignedReviewers(paperId);
        std::cout << "   Assigned " << reviewers.size() << " reviewers: ";
        for (size_t i = 0; i < reviewers.size(); ++i) {
            std::cout << reviewers[i] << (i < reviewers.size() - 1 ? ", " : "");
        }
        std::cout << std::endl;
    } else {
        std::cout << "❌ Failed to auto-assign reviewers (maybe no suitable candidates found)" << std::endl;
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
