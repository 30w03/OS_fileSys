#include "network/client.h"
#include "protocol/protocol.h"
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <vector>

void printUsage() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Peer Review System Commands" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n📡 Connection:" << std::endl;
    std::cout << "  connect <host> <port>     - Connect to server" << std::endl;
    std::cout << "  disconnect                - Disconnect from server" << std::endl;
    std::cout << "  ping                      - Ping server" << std::endl;
    
    std::cout << "\n👤 User Management:" << std::endl;
    std::cout << "  register <user> <pass> <role> - Register (role: author/reviewer/editor)" << std::endl;
    std::cout << "  login <user> <pass>       - Login to system" << std::endl;
    std::cout << "  logout                    - Logout" << std::endl;
    std::cout << "  profile <inst> <interests> - Update profile (institution, interests)" << std::endl;
    
    std::cout << "\n📝 Paper Management:" << std::endl;
    std::cout << "  submit <title> <abstract> <keywords> <file> - Submit a paper" << std::endl;
    std::cout << "  update_paper <paper_id> <file> - Update paper file (Overwrite)" << std::endl;
    std::cout << "  mypapers                  - View my submitted papers" << std::endl;
    std::cout << "  allpapers                 - View all papers (editor only)" << std::endl;
    
    std::cout << "\n🔍 Review Management:" << std::endl;
    std::cout << "  toreview                  - View papers assigned to me for review" << std::endl;
    std::cout << "  myreviews                 - View my review history" << std::endl; // 🔥 New
    std::cout << "  review <paper_id> <decision> <score> <comment>" << std::endl;
    std::cout << "         - Submit review (decision: accept/reject/revise)" << std::endl;
    
    std::cout << "\n👨‍💼 Editor Functions:" << std::endl;
    std::cout << "  assign <paper_id> <reviewer_id> - Assign reviewer to paper" << std::endl;
    std::cout << "  autoassign <paper_id>     - Automatically assign reviewers" << std::endl;
    std::cout << "  stats                     - View system statistics" << std::endl;
    std::cout << "  decision <paper_id> <decision> - Make editor decision (accept/reject/revise)" << std::endl;
    std::cout << "  download_paper <paper_id> - Download paper PDF" << std::endl;
    
    std::cout << "\n📁 File Operations:" << std::endl;
    std::cout << "  list                      - List remote files" << std::endl;
    std::cout << "  upload <local> <remote>   - Upload file" << std::endl;
    std::cout << "  download <remote> <local> - Download file" << std::endl;
    std::cout << "  delete <remote>           - Delete remote file" << std::endl;
    std::cout << "  revision <paper_id> <file> - Upload revised paper" << std::endl;

    std::cout << "\n🔧 Admin Functions:" << std::endl;
    std::cout << "  role <user_id> <role>     - Update user role" << std::endl;
    std::cout << "  deactivate <user_id>      - Deactivate user" << std::endl;
    std::cout << "  backup                    - Trigger system backup" << std::endl;
    
    std::cout << "\n❓ Other:" << std::endl;
    std::cout << "  help                      - Show this help" << std::endl;
    std::cout << "  quit                      - Exit client" << std::endl;
    
    std::cout << "\n🔒 Role Permissions:" << std::endl;
    std::cout << "  Author: Submit papers, view own papers" << std::endl;
    std::cout << "  Reviewer: Review assigned papers" << std::endl;
    std::cout << "  Editor: Assign reviewers, view all papers, view stats" << std::endl;
    std::cout << "  Admin: Manage users, monitor system" << std::endl;
    
    std::cout << "========================================\n" << std::endl;
}

void printPaper(const PaperInfo& paper) {
    std::cout << "\n┌─────────────────────────────────────────" << std::endl;
    std::cout << "│ Paper ID: " << paper.paperId << std::endl;
    std::cout << "│ Title: " << paper.title << std::endl;
    std::cout << "│ Abstract: " << paper.abstract << std::endl;
    std::cout << "│ Status: " << paper.status << std::endl;
    std::cout << "│ Version: " << paper.currentVersion << std::endl;
    std::cout << "│ Authors: " << paper.authorIds.size() << std::endl;
    std::cout << "│ Reviewers: " << paper.reviewerIds.size() << std::endl;
    
    // 显示评论
    if (!paper.reviews.empty()) {
        std::cout << "│" << std::endl;
        std::cout << "│ 📝 Reviews (" << paper.reviews.size() << "):" << std::endl;
        
        for (const auto& review : paper.reviews) {
            std::cout << "│   " << review.decision << " (Confidence: " << review.confidenceScore << "/5)" << std::endl;
            if (!review.comments.empty()) {
                std::cout << "│   Comments: \"" << review.comments << "\"" << std::endl;
            }
        }
    }
    
    std::cout << "└─────────────────────────────────────────\n" << std::endl;
}

int main() {
    Client client;
    std::string line;
    
    std::cout << "\n╔════════════════════════════════════════╗" << std::endl;
    std::cout << "║  Peer Review System Client v1.0        ║" << std::endl;
    std::cout << "╚════════════════════════════════════════╝" << std::endl;
    printUsage();
    
    while (true) {
        std::cout << "\n�� > ";
        if (!std::getline(std::cin, line)) {
            break;
        }
        
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        
        if (command.empty()) {
            continue;
        }
        
        if (command == "quit" || command == "exit") {
            std::cout << "\n👋 Goodbye!\n" << std::endl;
            break;
        }
        
        if (command == "help") {
            printUsage();
            continue;
        }
        
        if (command == "connect") {
            std::string host;
            int port;
            iss >> host >> port;
            
            if (host.empty() || port == 0) {
                std::cerr << "❌ Usage: connect <host> <port>" << std::endl;
                continue;
            }
            
            if (client.connect(host, port)) {
                std::cout << "✅ Connected to " << host << ":" << port << std::endl;
            } else {
                std::cerr << "❌ Failed to connect" << std::endl;
            }
            continue;
        }
        
        if (command == "disconnect") {
            client.disconnect();
            std::cout << "✅ Disconnected" << std::endl;
            continue;
        }
        
        if (command == "ping") {
            if (client.ping()) {
                std::cout << "✅ Server is alive" << std::endl;
            } else {
                std::cerr << "❌ Server not responding" << std::endl;
            }
            continue;
        }
        
        if (command == "register") {
            std::string username, password, role;
            iss >> username >> password >> role;
            
            if (username.empty() || password.empty() || role.empty()) {
                std::cerr << "❌ Usage: register <username> <password> <role>" << std::endl;
                std::cerr << "   Roles: author, reviewer, editor" << std::endl;
                continue;
            }
            
            if (client.registerUser(username, password, role)) {
                std::cout << "✅ User registered successfully" << std::endl;
            } else {
                std::cerr << "❌ Registration failed" << std::endl;
            }
            continue;
        }
        
        if (command == "login") {
            std::string username, password;
            iss >> username >> password;
            
            if (username.empty() || password.empty()) {
                std::cerr << "❌ Usage: login <username> <password>" << std::endl;
                continue;
            }
            
            if (client.login(username, password)) {
                std::cout << "✅ Login successful" << std::endl;
            } else {
                std::cerr << "❌ Login failed" << std::endl;
            }
            continue;
        }
        
        if (command == "logout") {
            if (client.logout()) {
                std::cout << "✅ Logged out successfully" << std::endl;
            } else {
                std::cerr << "❌ Logout failed" << std::endl;
            }
            continue;
        }
        
        if (command == "submit") {
            std::string title, abstract, keywords_str, filename;
            iss >> std::ws;
            std::getline(iss, title, '"');
            std::getline(iss, title, '"');
            iss >> std::ws;
            std::getline(iss, abstract, '"');
            std::getline(iss, abstract, '"');
            iss >> std::ws;
            
            if (iss.peek() == '"') {
                 std::getline(iss, keywords_str, '"');
                 std::getline(iss, keywords_str, '"');
                 iss >> filename;
            } else {
                 iss >> filename;
            }
            
            if (title.empty() || abstract.empty() || filename.empty()) {
                std::cerr << "❌ Usage: submit \"<title>\" \"<abstract>\" \"<keywords>\" <file>" << std::endl;
                continue;
            }
            
            std::vector<std::string> keywords;
            if (!keywords_str.empty()) {
                std::stringstream ks(keywords_str);
                std::string segment;
                while(std::getline(ks, segment, ',')) {
                    size_t first = segment.find_first_not_of(' ');
                    if (std::string::npos == first) continue;
                    size_t last = segment.find_last_not_of(' ');
                    keywords.push_back(segment.substr(first, (last - first + 1)));
                }
            }
            
            // Read file content
            std::ifstream file(filename, std::ios::binary);
            if (!file) {
                std::cerr << "❌ Cannot open file: " << filename << std::endl;
                continue;
            }
            
            std::vector<char> content((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
            
            if (client.submitPaper(title, abstract, content, keywords)) {
                std::cout << "✅ Paper submitted successfully" << std::endl;
            } else {
                std::cerr << "❌ Paper submission failed" << std::endl;
            }
            continue;
        }
        
        if (command == "mypapers") {
            auto papers = client.getMyPapers();
            if (papers.empty()) {
                std::cout << "📭 No papers found" << std::endl;
            } else {
                std::cout << "\n📚 Your Papers (" << papers.size() << "):" << std::endl;
                for (const auto& paper : papers) {
                    printPaper(paper);
                }
            }
            continue;
        }
        
        if (command == "toreview") {
            auto papers = client.getPapersToReview();
            if (papers.empty()) {
                std::cout << "📭 No papers assigned for review" << std::endl;
            } else {
                std::cout << "\n🔍 Papers to Review (" << papers.size() << "):" << std::endl;
                for (const auto& paper : papers) {
                    printPaper(paper);
                }
            }
            continue;
        }
        
        if (command == "review") {
            uint32_t paper_id, score;
            std::string decision, comment;
            iss >> paper_id >> decision >> score;
            iss >> std::ws;
            std::getline(iss, comment, '"');
            std::getline(iss, comment, '"');
            
            if (paper_id == 0 || decision.empty() || comment.empty()) {
                std::cerr << "❌ Usage: review <paper_id> <decision> <score> \"<comment>\"" << std::endl;
                std::cerr << "   Decisions: accept, reject, revise" << std::endl;
                continue;
            }
            
            if (client.submitReview(paper_id, decision, score, comment)) {
                std::cout << "✅ Review submitted successfully" << std::endl;
            } else {
                std::cerr << "❌ Review submission failed" << std::endl;
            }
            continue;
        }
        
        // 🔥 新增：查看审稿历史
        if (command == "myreviews") {
            auto reviews = client.getReviewerHistory();
            if (reviews.empty()) {
                std::cout << "📭 No review history found" << std::endl;
            } else {
                std::cout << "\n📝 Your Review History (" << reviews.size() << "):" << std::endl;
                for (const auto& review : reviews) {
                    std::cout << "┌─────────────────────────────────────────" << std::endl;
                    std::cout << "│ Review ID: " << review.reviewId << std::endl;
                    std::cout << "│ Paper ID: " << review.paperId << std::endl;
                    std::cout << "│ Decision: " << review.decision << std::endl;
                    std::cout << "│ Confidence: " << review.confidenceScore << "/5" << std::endl;
                    std::cout << "│ Comments: " << review.comments << std::endl;
                    std::cout << "└─────────────────────────────────────────\n" << std::endl;
                }
            }
            continue;
        }

        if (command == "assign") {
            uint32_t paper_id, reviewer_id;
            iss >> paper_id >> reviewer_id;
            
            if (paper_id == 0 || reviewer_id == 0) {
                std::cerr << "❌ Usage: assign <paper_id> <reviewer_id>" << std::endl;
                continue;
            }
            
            if (client.assignReviewer(paper_id, reviewer_id)) {
                std::cout << "✅ Reviewer assigned successfully" << std::endl;
            } else {
                std::cerr << "❌ Assignment failed" << std::endl;
            }
            continue;
        }

        if (command == "autoassign") {
            uint32_t paper_id;
            iss >> paper_id;
            
            if (paper_id == 0) {
                std::cerr << "❌ Usage: autoassign <paper_id>" << std::endl;
                continue;
            }
            
            if (client.autoAssignReviewers(paper_id)) {
                std::cout << "✅ Auto-assignment triggered successfully" << std::endl;
            } else {
                std::cerr << "❌ Auto-assignment failed" << std::endl;
            }
            continue;
        }

        if (command == "profile") {
            std::string institution, interests_str;
            iss >> std::ws;
            if (iss.peek() == '"') {
                std::getline(iss, institution, '"');
                std::getline(iss, institution, '"');
            } else {
                iss >> institution;
            }
            
            iss >> std::ws;
            if (iss.peek() == '"') {
                std::getline(iss, interests_str, '"');
                std::getline(iss, interests_str, '"');
            } else {
                std::getline(iss, interests_str);
            }

            if (institution.empty()) {
                 std::cerr << "❌ Usage: profile \"<institution>\" \"<interests>\"" << std::endl;
                 continue;
            }

            std::vector<std::string> interests;
            if (!interests_str.empty()) {
                std::stringstream ks(interests_str);
                std::string segment;
                while(std::getline(ks, segment, ',')) {
                    size_t first = segment.find_first_not_of(' ');
                    if (std::string::npos == first) continue;
                    size_t last = segment.find_last_not_of(' ');
                    interests.push_back(segment.substr(first, (last - first + 1)));
                }
            }
            
            if (client.updateProfile(institution, interests, 5)) {
                std::cout << "✅ Profile updated successfully" << std::endl;
            } else {
                std::cerr << "❌ Profile update failed" << std::endl;
            }
            continue;
        }
        
        if (command == "allpapers") {
            auto papers = client.getAllPapers();
            if (papers.empty()) {
                std::cout << "📭 No papers in system" << std::endl;
            } else {
                std::cout << "\n📚 All Papers (" << papers.size() << "):" << std::endl;
                for (const auto& paper : papers) {
                    printPaper(paper);
                }
            }
            continue;
        }
        
        if (command == "stats") {
            auto stats = client.getStatistics();
            std::cout << "\n📊 System Statistics:" << std::endl;
            std::cout << "┌─────────────────────────────────────────" << std::endl;
            std::cout << "│ Total Papers: " << stats.total_papers << std::endl;
            std::cout << "│ Total Reviews: " << stats.total_reviews << std::endl;
            std::cout << "│ Total Users: " << stats.total_users << std::endl;
            std::cout << "│ Pending Papers: " << stats.pending_papers << std::endl;
            std::cout << "│ Accepted Papers: " << stats.accepted_papers << std::endl;
            std::cout << "│ Rejected Papers: " << stats.rejected_papers << std::endl;
            std::cout << "└─────────────────────────────────────────\n" << std::endl;
            continue;
        }
        
        if (command == "list") {
            auto files = client.listFiles();
            if (files.empty()) {
                std::cout << "📁 No files found" << std::endl;
            } else {
                std::cout << "\n📁 Remote Files:" << std::endl;
                for (const auto& file : files) {
                    std::cout << "  📄 " << file.filename 
                             << " (" << file.size << " bytes)" << std::endl;
                }
            }
            continue;
        }
        
        if (command == "upload") {
            std::string local_path, remote_path;
            iss >> local_path >> remote_path;
            
            if (local_path.empty() || remote_path.empty()) {
                std::cerr << "❌ Usage: upload <local_path> <remote_path>" << std::endl;
                continue;
            }
            
            if (client.uploadFile(local_path, remote_path)) {
                std::cout << "✅ File uploaded successfully" << std::endl;
            } else {
                std::cerr << "❌ Upload failed" << std::endl;
            }
            continue;
        }
        
        if (command == "download") {
            std::string remote_path, local_path;
            iss >> remote_path >> local_path;
            
            if (remote_path.empty() || local_path.empty()) {
                std::cerr << "❌ Usage: download <remote_path> <local_path>" << std::endl;
                continue;
            }
            
            if (client.downloadFile(remote_path, local_path)) {
                std::cout << "✅ File downloaded successfully" << std::endl;
            } else {
                std::cerr << "❌ Download failed" << std::endl;
            }
            continue;
        }
        
        if (command == "delete") {
            std::string path;
            iss >> path;
            
            if (path.empty()) {
                std::cerr << "❌ Usage: delete <path>" << std::endl;
                continue;
            }
            
            if (client.deleteFile(path)) {
                std::cout << "✅ File deleted successfully" << std::endl;
            } else {
                std::cerr << "❌ Delete failed" << std::endl;
            }
            continue;
        }
        
        // ============================================================================
        // 🔥 新增命令
        // ============================================================================
        
        if (command == "revision") {
            uint32_t paper_id;
            std::string filename;
            iss >> paper_id >> filename;
            
            if (paper_id == 0 || filename.empty()) {
                std::cerr << "❌ Usage: revision <paper_id> <file_path>" << std::endl;
                continue;
            }
            
            std::ifstream file(filename, std::ios::binary);
            if (!file) {
                std::cerr << "❌ Cannot open file: " << filename << std::endl;
                continue;
            }
            
            std::vector<char> content((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
            
            if (client.uploadRevision(paper_id, content)) {
                std::cout << "✅ Revision uploaded successfully" << std::endl;
            } else {
                std::cerr << "❌ Revision upload failed" << std::endl;
            }
            continue;
        }

        if (command == "update_paper") {
            uint32_t paper_id;
            std::string filename;
            iss >> paper_id >> filename;
            
            if (paper_id == 0 || filename.empty()) {
                std::cerr << "❌ Usage: update_paper <paper_id> <file_path>" << std::endl;
                continue;
            }
            
            std::ifstream file(filename, std::ios::binary);
            if (!file) {
                std::cerr << "❌ Cannot open file: " << filename << std::endl;
                continue;
            }
            
            std::vector<char> content((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
            
            if (client.updatePaperFile(paper_id, content)) {
                std::cout << "✅ Paper updated successfully (Overwrite)" << std::endl;
            } else {
                std::cerr << "❌ Update failed" << std::endl;
            }
            continue;
        }
        
        if (command == "download_paper") {
            uint32_t paper_id;
            std::string local_path;
            iss >> paper_id >> local_path;
            
            if (paper_id == 0 || local_path.empty()) {
                std::cerr << "❌ Usage: download_paper <paper_id> <local_path>" << std::endl;
                continue;
            }
            
            if (client.downloadPaper(paper_id, local_path)) {
                std::cout << "✅ Paper downloaded successfully" << std::endl;
            } else {
                std::cerr << "❌ Paper download failed" << std::endl;
            }
            continue;
        }
        
        if (command == "decision") {
            uint32_t paper_id;
            std::string decision;
            iss >> paper_id >> decision;
            
            if (paper_id == 0 || decision.empty()) {
                std::cerr << "❌ Usage: decision <paper_id> <decision>" << std::endl;
                std::cerr << "   Decisions: ACCEPTED, REJECTED" << std::endl;
                continue;
            }
            
            if (client.makeDecision(paper_id, decision)) {
                std::cout << "✅ Decision recorded successfully" << std::endl;
            } else {
                std::cerr << "❌ Failed to record decision" << std::endl;
            }
            continue;
        }
        
        if (command == "role") {
            uint32_t user_id;
            std::string role;
            iss >> user_id >> role;
            
            if (user_id == 0 || role.empty()) {
                std::cerr << "❌ Usage: role <user_id> <role>" << std::endl;
                std::cerr << "   Roles: AUTHOR, REVIEWER, EDITOR, ADMIN" << std::endl;
                continue;
            }
            
            if (client.updateUserRole(user_id, role)) {
                std::cout << "✅ User role updated successfully" << std::endl;
            } else {
                std::cerr << "❌ Failed to update user role" << std::endl;
            }
            continue;
        }
        
        if (command == "deactivate") {
            uint32_t user_id;
            iss >> user_id;
            
            if (user_id == 0) {
                std::cerr << "❌ Usage: deactivate <user_id>" << std::endl;
                continue;
            }
            
            if (client.deactivateUser(user_id)) {
                std::cout << "✅ User deactivated successfully" << std::endl;
            } else {
                std::cerr << "❌ Failed to deactivate user" << std::endl;
            }
            continue;
        }
        
        if (command == "backup") {
            if (client.systemBackup()) {
                std::cout << "✅ System backup completed successfully" << std::endl;
            } else {
                std::cerr << "❌ System backup failed" << std::endl;
            }
            continue;
        }
        
        std::cerr << "❌ Unknown command: " << command << std::endl;
        std::cerr << "   Type 'help' for available commands" << std::endl;
    }
    
    return 0;
}
