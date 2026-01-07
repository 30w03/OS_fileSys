#pragma once
#include "paper.h"
#include "review.h"
#include "../filesystem/filesystem.h"
#include "../user/user_manager.h"
#include <memory>
#include <map>
#include <vector>
#include <mutex>

class ReviewSystem {
public:
    ReviewSystem(std::shared_ptr<Filesystem> fs, 
                 std::shared_ptr<UserManager> userMgr);
    ~ReviewSystem();
    
    bool init();

    // 论文操作
    uint32_t submitPaper(uint32_t authorId, const std::string& title,
                        const std::string& abstract, 
                        const std::vector<char>& fileData,
                        const std::vector<std::string>& keywords = {});
    // 修改论文（覆盖文件）
    bool updatePaperFile(uint32_t paperId, uint32_t authorId,
                        const std::vector<char>& fileData);
    bool uploadRevision(uint32_t paperId, uint32_t authorId,
                       const std::vector<char>& fileData);
    std::vector<Paper> getPapersByAuthor(uint32_t authorId);
    Paper getPaperInfo(uint32_t paperId);
    std::vector<Paper> getAllPapers();  // 编辑查看所有论文
    
    // 审稿人分配（编辑操作）
    bool assignReviewer(uint32_t editorId, uint32_t paperId, 
                       uint32_t reviewerId);
    bool removeReviewer(uint32_t editorId, uint32_t paperId, 
                       uint32_t reviewerId);
    
    // 自动分配审稿人
    bool autoAssignReviewers(uint32_t paperId);

    std::vector<uint32_t> getAssignedReviewers(uint32_t paperId);
    std::vector<Paper> getPapersForReviewer(uint32_t reviewerId);
    // 获取待审论文（只返回未审的）
    std::vector<Paper> getPapersToReview(uint32_t reviewerId) const;
    
    // 评审操作
    uint32_t submitReview(uint32_t reviewerId, uint32_t paperId,
                         ReviewDecision decision, int confidence,
                         const std::string& comments,
                         const std::vector<char>& fileData = {},
                         const std::string& filename = "");
    std::vector<Review> getReviewsForPaper(uint32_t paperId);
    std::vector<Review> getReviewsByReviewer(uint32_t reviewerId); // 🔥 New method
    Review getReviewInfo(uint32_t reviewId);
    
    // 编辑决定
    bool makeFinalDecision(uint32_t editorId, uint32_t paperId, 
                          PaperStatus decision);
    
    // 下载文件
    bool downloadPaper(uint32_t paperId, std::vector<char>& data);
    bool downloadReview(uint32_t reviewId, std::vector<char>& data);
    
    // 统计信息
    std::map<PaperStatus, int> getStatistics();
    uint32_t getReviewCount() const; // 获取评审总数
    void printStatistics();
    
    // 持久化
    bool saveMetadata();
    bool loadMetadata();

    // 权限检查 (Public for HttpHandler)
    bool isAuthorOfPaper(uint32_t userId, uint32_t paperId);
    bool isReviewerOfPaper(uint32_t userId, uint32_t paperId);
    bool isEditor(uint32_t userId);
    
private:
    std::shared_ptr<Filesystem> filesystem_;
    std::shared_ptr<UserManager> userManager_;
    
    std::map<uint32_t, Paper> papers_;
    std::map<uint32_t, Review> reviews_;
    
    uint32_t nextPaperId_;
    uint32_t nextReviewId_;
    
    mutable std::mutex mutex_;  // 并发保护
    
    // 内部辅助函数
    std::string generatePaperPath(uint32_t paperId, uint32_t version = 1);
    std::string generateReviewPath(uint32_t reviewId, const std::string& extension = ".txt");
    std::string generateMetadataPath();
    bool saveMetadataNoLock();
    
    // 序列化辅助函数
    bool serializePaper(const Paper& paper, std::vector<char>& data);
    bool deserializePaper(const std::vector<char>& data, Paper& paper);
    bool serializeReview(const Review& review, std::vector<char>& data);
    bool deserializeReview(const std::vector<char>& data, Review& review);
};
