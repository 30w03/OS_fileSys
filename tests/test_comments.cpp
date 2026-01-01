#include "user/user_manager.h"
#include "review/review_system.h"
#include "filesystem/filesystem.h"
#include <iostream>
#include <cassert>
#include <cstdio>
#include <cstring>

void testComments() {
    std::cout << "Testing Comments Functionality..." << std::endl;
    const std::string diskImage = "test_comments.img";
    
    // Cleanup
    remove(diskImage.c_str());
    
    // Setup FS
    auto fs = std::make_shared<Filesystem>(diskImage);
    fs->mount();
    
    auto um = std::make_shared<UserManager>();
    
    // Create users
    um->createUser("author", "pass1", UserRole::AUTHOR);
    um->createUser("reviewer", "pass2", UserRole::REVIEWER);
    um->createUser("editor", "pass3", UserRole::EDITOR);
    
    uint32_t authorId, reviewerId, editorId;
    um->authenticateUser("author", "pass1", authorId);
    um->authenticateUser("reviewer", "pass2", reviewerId);
    um->authenticateUser("editor", "pass3", editorId);
    
    ReviewSystem rs(fs, um);
    
    // Submit paper
    std::string title = "Test Paper with Comments";
    std::string abstract = "This is a test abstract.";
    std::vector<char> content = {'H', 'e', 'l', 'l', 'o'};
    
    uint32_t paperId = rs.submitPaper(authorId, title, abstract, content);
    assert(paperId > 0);
    std::cout << "  Submitted paper ID: " << paperId << std::endl;
    
    // Assign reviewer
    bool assigned = rs.assignReviewer(editorId, paperId, reviewerId);
    assert(assigned);
    std::cout << "  Assigned reviewer ID: " << reviewerId << std::endl;
    
    // Submit review with comments
    std::string comments = "This is a detailed review comment. The paper is well-written.";
    uint32_t reviewId = rs.submitReview(reviewerId, paperId, ReviewDecision::ACCEPT, 4, comments);
    assert(reviewId > 0);
    std::cout << "  Submitted review ID: " << reviewId << std::endl;
    
    // Get reviews for the paper
    std::vector<Review> reviews = rs.getReviewsForPaper(paperId);
    assert(reviews.size() == 1);
    assert(reviews[0].comments == comments);
    std::cout << "  Review comments: " << reviews[0].comments << std::endl;
    
    // Verify comments are preserved in metadata
    rs.saveMetadata();
    
    // Recreate ReviewSystem to test persistence
    ReviewSystem rs2(fs, um);
    std::vector<Review> reviews2 = rs2.getReviewsForPaper(paperId);
    assert(reviews2.size() == 1);
    assert(reviews2[0].comments == comments);
    std::cout << "  Persisted review comments: " << reviews2[0].comments << std::endl;
    
    std::cout << "✅ Comments Functionality Test Passed" << std::endl;
    remove(diskImage.c_str());
}

int main() {
    testComments();
    return 0;
}