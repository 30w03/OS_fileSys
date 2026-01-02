#include "review/review_system.h"
#include "user/user_manager.h"
#include "filesystem/filesystem.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <memory>

// 简单的断言宏，打印更多信息
#define ASSERT_TRUE(cond, msg) \
    if (!(cond)) { \
        std::cerr << "❌ Test Failed: " << msg << " (Line " << __LINE__ << ")" << std::endl; \
        return 1; \
    } else { \
        std::cout << "✅ " << msg << std::endl; \
    }

int main() {
    std::cout << "Starting Auto Assignment Tests..." << std::endl;

    // 1. Setup
    auto fs = std::make_shared<Filesystem>("test_assign.img");
    fs->format(4096, 100);
    fs->mount();
    
    // Initialize directories
    auto dirOps = fs->getDirOps();
    dirOps->mkdir("/papers");
    dirOps->mkdir("/reviews");
    dirOps->mkdir("/system");
    
    auto userMgr = std::make_shared<UserManager>();
    auto reviewSys = std::make_shared<ReviewSystem>(fs, userMgr);
    
    // 2. Create Users
    // Author
    userMgr->createUser("author", "pass", UserRole::AUTHOR);
    uint32_t authorId;
    userMgr->authenticateUser("author", "pass", authorId);
    userMgr->updateUserProfile(authorId, "UnivA", {"AI"}, 3);
    
    // Reviewer 1 (CoI: Same Inst)
    userMgr->createUser("rev1", "pass", UserRole::REVIEWER);
    uint32_t r1;
    userMgr->authenticateUser("rev1", "pass", r1);
    userMgr->updateUserProfile(r1, "UnivA", {"AI"}, 3);
    
    // Reviewer 2 (Good Match)
    userMgr->createUser("rev2", "pass", UserRole::REVIEWER);
    uint32_t r2;
    userMgr->authenticateUser("rev2", "pass", r2);
    userMgr->updateUserProfile(r2, "UnivB", {"AI"}, 3);
    
    // Reviewer 3 (Bad Match)
    userMgr->createUser("rev3", "pass", UserRole::REVIEWER);
    uint32_t r3;
    userMgr->authenticateUser("rev3", "pass", r3);
    userMgr->updateUserProfile(r3, "UnivB", {"Systems"}, 3);
    
    // Reviewer 4 (Good Match)
    userMgr->createUser("rev4", "pass", UserRole::REVIEWER);
    uint32_t r4;
    userMgr->authenticateUser("rev4", "pass", r4);
    userMgr->updateUserProfile(r4, "UnivC", {"AI"}, 3);
    
    // Reviewer 5 (Good Match)
    userMgr->createUser("rev5", "pass", UserRole::REVIEWER);
    uint32_t r5;
    userMgr->authenticateUser("rev5", "pass", r5);
    userMgr->updateUserProfile(r5, "UnivD", {"AI"}, 3);

    // 3. Submit Paper
    std::vector<char> dummyData = {'A', 'B', 'C'};
    uint32_t paperId = reviewSys->submitPaper(authorId, "AI Paper", "Abstract", dummyData, {"AI"});
    ASSERT_TRUE(paperId > 0, "Paper submitted");
    
    // 4. Auto Assign
    bool result = reviewSys->autoAssignReviewers(paperId);
    ASSERT_TRUE(result, "Auto assign executed");
    
    // 5. Verify
    auto assigned = reviewSys->getAssignedReviewers(paperId);
    std::cout << "Assigned reviewers count: " << assigned.size() << std::endl;
    ASSERT_TRUE(assigned.size() == 3, "Assigned 3 reviewers");
    
    bool r1_assigned = false;
    bool r2_assigned = false;
    bool r3_assigned = false;
    bool r4_assigned = false;
    bool r5_assigned = false;
    
    for (uint32_t id : assigned) {
        if (id == r1) r1_assigned = true;
        if (id == r2) r2_assigned = true;
        if (id == r3) r3_assigned = true;
        if (id == r4) r4_assigned = true;
        if (id == r5) r5_assigned = true;
    }
    
    ASSERT_TRUE(!r1_assigned, "Reviewer 1 (CoI) not assigned");
    ASSERT_TRUE(r2_assigned, "Reviewer 2 (Match) assigned");
    ASSERT_TRUE(r4_assigned, "Reviewer 4 (Match) assigned");
    ASSERT_TRUE(r5_assigned, "Reviewer 5 (Match) assigned");
    ASSERT_TRUE(!r3_assigned, "Reviewer 3 (No Match) not assigned");
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}