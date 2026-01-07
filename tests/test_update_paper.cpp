#include "review/review_system.h"
#include "user/user_manager.h"
#include "filesystem/filesystem.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cstring>

// Helper to check file content
bool checkFileContent(std::shared_ptr<Filesystem> fs, const std::string& path, const std::string& expected) {
    auto fileOps = fs->getFileOps();
    // Assuming root (1) for simplicity or matching authorId if we knew it
    // The path is absolute so explicit owner check might be bypassed or we simulate it.
    // Filesystem read helper:
    std::vector<char> data;
    if (!fs->readFile(path, data)) return false;
    std::string content(data.begin(), data.end());
    return content == expected;
}

int main() {
    // Setup environment
    std::system("rm -f test_update.img users_test.dat");
    
    auto disk = "test_update.img";
    auto fs = std::make_shared<Filesystem>(disk);
    fs->format();
    fs->mount();
    
    // Create necessary directories
    auto dirOps = fs->getDirOps();
    dirOps->mkdir("/papers");
    dirOps->mkdir("/reviews");
    dirOps->mkdir("/system");
    
    auto userMgr = std::make_shared<UserManager>();
    userMgr->loadFromFile("users_test.dat"); // Fresh
    
    // Create Author
    userMgr->createUser("author", "pass", UserRole::AUTHOR);
    uint32_t authorId;
    userMgr->authenticateUser("author", "pass", authorId);
    
    auto reviewSys = std::make_shared<ReviewSystem>(fs, userMgr);
    reviewSys->init();
    
    // Submit Paper
    std::string title = "Original Paper";
    std::string abstract = "Abstract";
    std::string content1 = "Version 1 Content";
    std::vector<char> data1(content1.begin(), content1.end());
    
    uint32_t paperId = reviewSys->submitPaper(authorId, title, abstract, data1);
    assert(paperId > 0);
    std::cout << "Paper submitted with ID: " << paperId << std::endl;
    
    // Verify Content 1
    Paper p1 = reviewSys->getPaperInfo(paperId);
    if (!checkFileContent(fs, p1.filepath, content1)) {
        std::cerr << "FAILED: Initial content mismatch" << std::endl;
        return 1;
    }
    
    // Update Paper
    std::string content2 = "Version 2 Content (Updated)";
    std::vector<char> data2(content2.begin(), content2.end());
    
    bool result = reviewSys->updatePaperFile(paperId, authorId, data2);
    if (!result) {
        std::cerr << "FAILED: updatePaperFile returned false" << std::endl;
        return 1;
    }
    
    // Verify Content 2
    if (!checkFileContent(fs, p1.filepath, content2)) {
        std::cerr << "FAILED: Updated content mismatch" << std::endl;
        // Debug: print what we got
        std::vector<char> readData;
        fs->readFile(p1.filepath, readData);
        std::string actual(readData.begin(), readData.end());
        std::cerr << "Expected: " << content2 << "\nActual: " << actual << std::endl;
        return 1;
    }
    
    // Verify Metadata (should still be SUBMITTED)
    Paper p2 = reviewSys->getPaperInfo(paperId);
    assert(p2.status == PaperStatus::SUBMITTED);
    
    std::cout << "SUCCESS: Paper updated successfully" << std::endl;
    
    // Clean up
    std::system("rm -f test_update.img users_test.dat");
    return 0;
}
