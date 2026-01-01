#include "user/user_manager.h"
#include "review/review_system.h"
#include "filesystem/filesystem.h"
#include <iostream>
#include <cassert>
#include <cstdio>
#include <cstring>

void testUserPersistence() {
    std::cout << "Testing User Persistence..." << std::endl;
    const std::string userFile = "test_users.dat";
    
    // Cleanup
    remove(userFile.c_str());
    
    {
        UserManager um;
        // Create a user
        bool created = um.createUser("testuser", "password123", UserRole::AUTHOR);
        assert(created);
        
        // Verify user exists
        uint32_t userId;
        bool auth = um.authenticateUser("testuser", "password123", userId);
        assert(auth);
        
        // Save
        bool saved = um.saveToFile(userFile);
        assert(saved);
        std::cout << "  Saved users to " << userFile << std::endl;
    }
    
    {
        UserManager um;
        // Load
        bool loaded = um.loadFromFile(userFile);
        assert(loaded);
        
        // Verify user exists
        uint32_t userId;
        bool auth = um.authenticateUser("testuser", "password123", userId);
        assert(auth);
        std::cout << "  Authenticated loaded user successfully. UserID: " << userId << std::endl;
        
        User user;
        bool gotUser = um.getUserById(userId, user);
        assert(gotUser);
        assert(user.username == "testuser");
        assert(user.role == UserRole::AUTHOR);
    }
    
    std::cout << "✅ User Persistence Test Passed" << std::endl;
    remove(userFile.c_str());
}

void testReviewPersistence() {
    std::cout << "Testing Review Persistence..." << std::endl;
    const std::string diskImage = "test_persistence.img";
    
    // Cleanup
    remove(diskImage.c_str());
    
    // Setup FS
    auto fs = std::make_shared<Filesystem>(diskImage);
    // fs->format(); // Not needed/available
    fs->mount();
    
    auto um = std::make_shared<UserManager>();
    um->createUser("author1", "pass", UserRole::AUTHOR);
    uint32_t authorId;
    um->authenticateUser("author1", "pass", authorId);
    
    {
        ReviewSystem rs(fs, um);
        
        // Submit paper
        std::string title = "Test Paper";
        std::string abstract = "This is a test abstract.";
        std::vector<char> content = {'H', 'e', 'l', 'l', 'o'};
        
        uint32_t paperId = rs.submitPaper(authorId, title, abstract, content);
        assert(paperId > 0);
        std::cout << "  Submitted paper ID: " << paperId << std::endl;
        
        // Save metadata
        bool saved = rs.saveMetadata();
        assert(saved);
        std::cout << "  Saved review metadata" << std::endl;
    }
    
    {
        // Re-create ReviewSystem
        ReviewSystem rs(fs, um);
        // Metadata should be loaded in constructor
        
        // Verify paper exists
        Paper paper = rs.getPaperInfo(1); // Assuming ID starts at 1
        assert(paper.paperId == 1);
        assert(paper.title == "Test Paper");
        assert(paper.abstract == "This is a test abstract.");
        assert(paper.authorIds.size() > 0);
        assert(paper.authorIds[0] == authorId);
        
        std::cout << "  Verified loaded paper: " << paper.title << std::endl;
    }
    
    std::cout << "✅ Review Persistence Test Passed" << std::endl;
    remove(diskImage.c_str());
}

int main() {
    testUserPersistence();
    testReviewPersistence();
    return 0;
}
