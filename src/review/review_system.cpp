#include "review/review_system.h"
#include "user/user.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>
#include <cstring>

ReviewSystem::ReviewSystem(std::shared_ptr<Filesystem> fs, 
                          std::shared_ptr<UserManager> userMgr)
    : filesystem_(fs)
    , userManager_(userMgr)
    , nextPaperId_(1)
    , nextReviewId_(1) {
    
    loadMetadata();
}

ReviewSystem::~ReviewSystem() {
    // saveMetadata();  // TEMP FIX
}

// ============================================================================
// 论文提交
// ============================================================================
uint32_t ReviewSystem::submitPaper(uint32_t authorId, 
                                  const std::string& title,
                                  const std::string& abstract,
                                  const std::vector<char>& fileData) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 验证用户存在
    User user;
    if (!userManager_->getUserById(authorId, user)) {
        std::cerr << "❌ Invalid user ID: " << authorId << std::endl;
        return 0;
    }
    
    // 创建论文对象
    Paper paper;
    paper.paperId = nextPaperId_++;
    paper.title = title;
    paper.abstract = abstract;
    paper.authorIds.push_back(authorId);
    paper.status = PaperStatus::SUBMITTED;
    paper.submissionTime = std::time(nullptr);
    paper.currentVersion = 1;
    
    // 生成文件路径
    paper.filepath = generatePaperPath(paper.paperId, 1);
    
    // 保存文件到文件系统
    if (!filesystem_->writeFile(paper.filepath, fileData)) {
        std::cerr << "❌ Failed to write paper file: " << paper.filepath << std::endl;
        nextPaperId_--;  // 回滚 ID
        return 0;
    }
    
    // 保存论文元数据
    papers_[paper.paperId] = paper;
    // saveMetadata();  // TEMP FIX
    
    std::cout << "✅ Paper submitted successfully!" << std::endl;
    std::cout << "   Paper ID: " << paper.paperId << std::endl;
    std::cout << "   Title: " << title << std::endl;
    std::cout << "   Author ID: " << authorId << std::endl;
    std::cout << "   File path: " << paper.filepath << std::endl;
    
    return paper.paperId;
}

// ============================================================================
// 上传修订版本
// ============================================================================
bool ReviewSystem::uploadRevision(uint32_t paperId, uint32_t authorId,
                                 const std::vector<char>& fileData) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = papers_.find(paperId);
    if (it == papers_.end()) {
        std::cerr << "❌ Paper not found: " << paperId << std::endl;
        return false;
    }
    
    Paper& paper = it->second;
    
    // 检查权限
    if (!isAuthorOfPaper(authorId, paperId)) {
        std::cerr << "❌ User " << authorId << " is not author of paper " << paperId << std::endl;
        return false;
    }
    
    // 生成新版本路径
    paper.currentVersion++;
    std::string revisionPath = generatePaperPath(paperId, paper.currentVersion);
    paper.revisionPaths.push_back(revisionPath);
    
    // 保存新版本
    if (!filesystem_->writeFile(revisionPath, fileData)) {
        std::cerr << "❌ Failed to write revision: " << revisionPath << std::endl;
        paper.currentVersion--;
        paper.revisionPaths.pop_back();
        return false;
    }
    
    // saveMetadata();  // TEMP FIX
    
    std::cout << "✅ Revision uploaded successfully!" << std::endl;
    std::cout << "   Paper ID: " << paperId << std::endl;
    std::cout << "   New version: " << paper.currentVersion << std::endl;
    
    return true;
}

// ============================================================================
// 获取作者的论文列表
// ============================================================================
std::vector<Paper> ReviewSystem::getPapersByAuthor(uint32_t authorId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Paper> result;
    for (const auto& pair : papers_) {
        const Paper& paper = pair.second;
        if (std::find(paper.authorIds.begin(), paper.authorIds.end(), authorId) 
            != paper.authorIds.end()) {
            result.push_back(paper);
        }
    }
    return result;
}

// ============================================================================
// 获取论文信息
// ============================================================================
Paper ReviewSystem::getPaperInfo(uint32_t paperId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = papers_.find(paperId);
    if (it != papers_.end()) {
        return it->second;
    }
    return Paper();
}

// ============================================================================
// 获取所有论文（编辑）
// ============================================================================
std::vector<Paper> ReviewSystem::getAllPapers() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Paper> result;
    for (const auto& pair : papers_) {
        result.push_back(pair.second);
    }
    return result;
}

// ============================================================================
// 分配审稿人
// ============================================================================
bool ReviewSystem::assignReviewer(uint32_t editorId, uint32_t paperId, 
                                 uint32_t reviewerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查编辑权限
    if (!isEditor(editorId)) {
        std::cerr << "❌ User " << editorId << " is not an editor" << std::endl;
        return false;
    }
    
    auto it = papers_.find(paperId);
    if (it == papers_.end()) {
        std::cerr << "❌ Paper not found: " << paperId << std::endl;
        return false;
    }
    
    Paper& paper = it->second;
    
    // 验证审稿人存在且有审稿人角色
    User reviewer;
    if (!userManager_->getUserById(reviewerId, reviewer)) {
        std::cerr << "❌ Reviewer not found: " << reviewerId << std::endl;
        return false;
    }
    
    if (reviewer.role != UserRole::REVIEWER && reviewer.role != UserRole::EDITOR) {
        std::cerr << "❌ User " << reviewerId << " is not a reviewer" << std::endl;
        return false;
    }
    
    // 检查是否已分配
    if (std::find(paper.assignedReviewers.begin(), 
                  paper.assignedReviewers.end(), 
                  reviewerId) != paper.assignedReviewers.end()) {
        std::cerr << "❌ Reviewer " << reviewerId << " already assigned to paper " << paperId << std::endl;
        return false;
    }
    
    paper.assignedReviewers.push_back(reviewerId);
    if (paper.status == PaperStatus::SUBMITTED) {
        paper.status = PaperStatus::UNDER_REVIEW;
    }
    
    // saveMetadata();  // TEMP FIX
    
    std::cout << "✅ Reviewer assigned successfully!" << std::endl;
    std::cout << "   Paper ID: " << paperId << std::endl;
    std::cout << "   Reviewer ID: " << reviewerId << std::endl;
    
    return true;
}

// ============================================================================
// 移除审稿人
// ============================================================================
bool ReviewSystem::removeReviewer(uint32_t editorId, uint32_t paperId, 
                                 uint32_t reviewerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isEditor(editorId)) {
        return false;
    }
    
    auto it = papers_.find(paperId);
    if (it == papers_.end()) {
        return false;
    }
    
    Paper& paper = it->second;
    
    auto reviewerIt = std::find(paper.assignedReviewers.begin(), 
                                paper.assignedReviewers.end(), 
                                reviewerId);
    
    if (reviewerIt == paper.assignedReviewers.end()) {
        return false;
    }
    
    paper.assignedReviewers.erase(reviewerIt);
    // saveMetadata();  // TEMP FIX
    
    return true;
}

// ============================================================================
// 获取分配的审稿人列表
// ============================================================================
std::vector<uint32_t> ReviewSystem::getAssignedReviewers(uint32_t paperId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = papers_.find(paperId);
    if (it != papers_.end()) {
        return it->second.assignedReviewers;
    }
    return std::vector<uint32_t>();
}

// ============================================================================
// 获取审稿人需要审的论文
// ============================================================================
std::vector<Paper> ReviewSystem::getPapersForReviewer(uint32_t reviewerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Paper> result;
    for (const auto& pair : papers_) {
        const Paper& paper = pair.second;
        if (std::find(paper.assignedReviewers.begin(), 
                     paper.assignedReviewers.end(), 
                     reviewerId) != paper.assignedReviewers.end()) {
            result.push_back(paper);
        }
    }
    return result;
}

// ============================================================================
// 🔧 修复问题1：获取待审论文（只返回未审的）
// ============================================================================
std::vector<Paper> ReviewSystem::getPapersToReview(uint32_t reviewerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Paper> result;
    
    for (const auto& [paperId, paper] : papers_) {
        // 检查是否分配给该审稿人
        bool isAssigned = std::find(paper.assignedReviewers.begin(), 
                                    paper.assignedReviewers.end(), 
                                    reviewerId) != paper.assignedReviewers.end();
        
        if (!isAssigned) continue;
        
        // 检查是否已提交评审
        bool hasReviewed = false;
        for (const auto& [reviewId, review] : reviews_) {
            if (review.paperId == paperId && review.reviewerId == reviewerId) {
                hasReviewed = true;
                break;
            }
        }
        
        // ✅ 只返回未审的论文
        if (!hasReviewed) {
            result.push_back(paper);
        }
    }
    
    return result;
}

// ============================================================================
// 🔧 修复问题2 & 问题4：提交评审并自动更新状态
// ============================================================================
uint32_t ReviewSystem::submitReview(uint32_t reviewerId, uint32_t paperId,
                                   ReviewDecision decision, int confidence,
                                   const std::string& comments) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isReviewerOfPaper(reviewerId, paperId)) {
        std::cerr << "❌ User " << reviewerId << " is not assigned to review paper " << paperId << std::endl;
        return 0;
    }
    
    // ✅ 检查是否已经提交过评审
    for (const auto& [rid, rev] : reviews_) {
        if (rev.paperId == paperId && rev.reviewerId == reviewerId) {
            std::cerr << "❌ Reviewer " << reviewerId << " has already submitted a review for paper " << paperId << std::endl;
            return 0;
        }
    }
    
    Review review;
    review.reviewId = nextReviewId_++;
    review.paperId = paperId;
    review.reviewerId = reviewerId;
    review.decision = decision;
    review.confidenceScore = confidence;
    review.comments = comments;
    review.submitTime = std::time(nullptr);
    review.filepath = generateReviewPath(review.reviewId);
    
    // 保存评审文件
    std::vector<char> reviewData(comments.begin(), comments.end());
    if (!filesystem_->writeFile(review.filepath, reviewData)) {
        std::cerr << "❌ Failed to write review file: " << review.filepath << std::endl;
        nextReviewId_--;
        return 0;
    }
    
    reviews_[review.reviewId] = review;
    
    // ✅ 修复问题2：根据审稿决定自动更新论文状态
    auto it = papers_.find(paperId);
    if (it != papers_.end()) {
        Paper& paper = it->second;
        
        // 检查是否所有审稿人都已提交
        int assignedCount = paper.assignedReviewers.size();
        int reviewCount = 0;
        
        for (const auto& [rid, rev] : reviews_) {
            if (rev.paperId == paperId) {
                reviewCount++;
            }
        }
        
        std::cout << "   📊 Review progress: " << reviewCount << "/" << assignedCount << std::endl;
        
        // 如果所有审稿人都提交了，根据决定更新状态
        if (reviewCount >= assignedCount && assignedCount > 0) {
            int acceptCount = 0;
            int rejectCount = 0;
            int borderlineCount = 0;
            
            // 统计所有审稿决定
            for (const auto& [rid, rev] : reviews_) {
                if (rev.paperId == paperId) {
                    if (rev.decision == ReviewDecision::STRONG_ACCEPT || 
                        rev.decision == ReviewDecision::ACCEPT ||
                        rev.decision == ReviewDecision::WEAK_ACCEPT) {
                        acceptCount++;
                    } else if (rev.decision == ReviewDecision::STRONG_REJECT || 
                               rev.decision == ReviewDecision::REJECT ||
                               rev.decision == ReviewDecision::WEAK_REJECT) {
                        rejectCount++;
                    } else {
                        borderlineCount++;
                    }
                }
            }
            
            // ✅ 决定最终状态（简单多数规则）
            if (rejectCount > 0) {
                paper.status = PaperStatus::REJECTED;
                std::cout << "   📋 Paper status updated: REJECTED" << std::endl;
            } else if (acceptCount == assignedCount) {
                paper.status = PaperStatus::ACCEPTED;
                std::cout << "   📋 Paper status updated: ACCEPTED" << std::endl;
            } else {
                paper.status = PaperStatus::REVIEWED;
                std::cout << "   📋 Paper status updated: REVIEWED (needs editor decision)" << std::endl;
            }
        }
    }
    
    // saveMetadata();  // TEMP FIX
    
    std::cout << "✅ Review submitted successfully!" << std::endl;
    std::cout << "   Review ID: " << review.reviewId << std::endl;
    std::cout << "   Paper ID: " << paperId << std::endl;
    std::cout << "   Decision: " << review.getDecisionName() << std::endl;
    
    return review.reviewId;
}

// ============================================================================
// 获取论文的所有评审
// ============================================================================
std::vector<Review> ReviewSystem::getReviewsForPaper(uint32_t paperId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Review> result;
    for (const auto& pair : reviews_) {
        if (pair.second.paperId == paperId) {
            result.push_back(pair.second);
        }
    }
    return result;
}

// ============================================================================
// 获取评审信息
// ============================================================================
Review ReviewSystem::getReviewInfo(uint32_t reviewId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = reviews_.find(reviewId);
    if (it != reviews_.end()) {
        return it->second;
    }
    return Review();
}

// ============================================================================
// 编辑做最终决定
// ============================================================================
bool ReviewSystem::makeFinalDecision(uint32_t editorId, uint32_t paperId, 
                                    PaperStatus decision) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isEditor(editorId)) {
        std::cerr << "❌ User " << editorId << " is not an editor" << std::endl;
        return false;
    }
    
    auto it = papers_.find(paperId);
    if (it == papers_.end()) {
        std::cerr << "❌ Paper not found: " << paperId << std::endl;
        return false;
    }
    
    it->second.status = decision;
    // saveMetadata();  // TEMP FIX
    
    std::cout << "✅ Final decision made!" << std::endl;
    std::cout << "   Paper ID: " << paperId << std::endl;
    std::cout << "   Decision: " << it->second.getStatusName() << std::endl;
    
    return true;
}

// ============================================================================
// 下载论文
// ============================================================================
bool ReviewSystem::downloadPaper(uint32_t paperId, std::vector<char>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = papers_.find(paperId);
    if (it == papers_.end()) {
        return false;
    }
    
    return filesystem_->readFile(it->second.filepath, data);
}

// ============================================================================
// 下载评审
// ============================================================================
bool ReviewSystem::downloadReview(uint32_t reviewId, std::vector<char>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = reviews_.find(reviewId);
    if (it == reviews_.end()) {
        return false;
    }
    
    return filesystem_->readFile(it->second.filepath, data);
}

// ============================================================================
// 🔧 修复问题4：正确的统计信息
// ============================================================================
std::map<PaperStatus, int> ReviewSystem::getStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::map<PaperStatus, int> stats;
    for (const auto& pair : papers_) {
        stats[pair.second.status]++;
    }
    return stats;
}

void ReviewSystem::printStatistics() {
    auto stats = getStatistics();
    
    std::cout << "\n========== REVIEW SYSTEM STATISTICS ==========" << std::endl;
    std::cout << "Total Papers: " << papers_.size() << std::endl;
    std::cout << "Total Reviews: " << reviews_.size() << std::endl;  // ✅ 修复：正确显示评审数量
    std::cout << "\nPapers by Status:" << std::endl;
    std::cout << "  Submitted:     " << stats[PaperStatus::SUBMITTED] << std::endl;
    std::cout << "  Under Review:  " << stats[PaperStatus::UNDER_REVIEW] << std::endl;
    std::cout << "  Reviewed:      " << stats[PaperStatus::REVIEWED] << std::endl;
    std::cout << "  Accepted:      " << stats[PaperStatus::ACCEPTED] << std::endl;
    std::cout << "  Rejected:      " << stats[PaperStatus::REJECTED] << std::endl;
    std::cout << "=============================================\n" << std::endl;
}

// ============================================================================
// 权限检查
// ============================================================================
bool ReviewSystem::isAuthorOfPaper(uint32_t userId, uint32_t paperId) {
    auto it = papers_.find(paperId);
    if (it == papers_.end()) {
        return false;
    }
    
    const auto& authors = it->second.authorIds;
    return std::find(authors.begin(), authors.end(), userId) != authors.end();
}

bool ReviewSystem::isReviewerOfPaper(uint32_t userId, uint32_t paperId) {
    auto it = papers_.find(paperId);
    if (it == papers_.end()) {
        return false;
    }
    
    const auto& reviewers = it->second.assignedReviewers;
    return std::find(reviewers.begin(), reviewers.end(), userId) != reviewers.end();
}

bool ReviewSystem::isEditor(uint32_t userId) {
    User user;
    if (!userManager_->getUserById(userId, user)) {
        return false;
    }
    return user.role == UserRole::EDITOR || user.role == UserRole::ADMIN;
}

// ============================================================================
// 路径生成
// ============================================================================
std::string ReviewSystem::generatePaperPath(uint32_t paperId, uint32_t version) {
    std::ostringstream oss;
    oss << "/papers/paper_" << std::setw(6) << std::setfill('0') << paperId;
    if (version > 1) {
        oss << "_v" << version;
    }
    oss << ".pdf";
    return oss.str();
}

std::string ReviewSystem::generateReviewPath(uint32_t reviewId) {
    std::ostringstream oss;
    oss << "/reviews/review_" << std::setw(6) << std::setfill('0') << reviewId << ".txt";
    return oss.str();
}

std::string ReviewSystem::generateMetadataPath() {
    return "/system/review_metadata.dat";
}

// ============================================================================
// 序列化辅助函数
// ============================================================================
bool ReviewSystem::serializePaper(const Paper& paper, std::vector<char>& data) {
    // TODO: 实现完整的序列化
    // 当前简化版本
    return true;
}

bool ReviewSystem::deserializePaper(const std::vector<char>& data, Paper& paper) {
    // TODO: 实现完整的反序列化
    return true;
}

bool ReviewSystem::serializeReview(const Review& review, std::vector<char>& data) {
    // TODO: 实现完整的序列化
    return true;
}

bool ReviewSystem::deserializeReview(const std::vector<char>& data, Review& review) {
    // TODO: 实现完整的反序列化
    return true;
}

// ============================================================================
// 持久化
// ============================================================================
bool ReviewSystem::saveMetadata() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::vector<char> data;
        
        // Helper lambdas
        auto writeU32 = [&](uint32_t val) {
            const char* ptr = reinterpret_cast<const char*>(&val);
            data.insert(data.end(), ptr, ptr + 4);
        };
        auto writeU64 = [&](uint64_t val) {
            const char* ptr = reinterpret_cast<const char*>(&val);
            data.insert(data.end(), ptr, ptr + 8);
        };
        auto writeString = [&](const std::string& str) {
            writeU32(str.length());
            data.insert(data.end(), str.begin(), str.end());
        };
        auto writeVecU32 = [&](const std::vector<uint32_t>& vec) {
            writeU32(vec.size());
            if (!vec.empty()) {
                const char* ptr = reinterpret_cast<const char*>(vec.data());
                data.insert(data.end(), ptr, ptr + vec.size() * 4);
            }
        };
        auto writeVecString = [&](const std::vector<std::string>& vec) {
            writeU32(vec.size());
            for (const auto& s : vec) writeString(s);
        };

        // 1. Counters
        writeU32(nextPaperId_);
        writeU32(nextReviewId_);
        
        // 2. Papers
        writeU32(papers_.size());
        for (const auto& pair : papers_) {
            const Paper& p = pair.second;
            writeU32(p.paperId);
            writeString(p.title);
            writeString(p.abstract);
            writeVecU32(p.authorIds);
            writeString(p.filepath);
            data.push_back(static_cast<char>(p.status));
            writeU64(static_cast<uint64_t>(p.submissionTime));
            writeVecU32(p.assignedReviewers);
            writeU32(p.currentVersion);
            writeVecString(p.revisionPaths);
        }
        
        // 3. Reviews
        writeU32(reviews_.size());
        for (const auto& pair : reviews_) {
            const Review& r = pair.second;
            writeU32(r.reviewId);
            writeU32(r.paperId);
            writeU32(r.reviewerId);
            data.push_back(static_cast<char>(r.decision));
            writeString(r.comments);
            writeU32(r.confidenceScore);
            writeU64(static_cast<uint64_t>(r.submitTime));
            writeString(r.filepath);
        }
        
        std::string metaPath = generateMetadataPath();
        return filesystem_->writeFile(metaPath, data);
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to save metadata: " << e.what() << std::endl;
        return false;
    }
}

bool ReviewSystem::loadMetadata() {
    try {
        std::string metaPath = generateMetadataPath();
        std::vector<char> data;
        
        if (!filesystem_->readFile(metaPath, data)) {
            std::cout << "ℹ️  No existing metadata found, starting fresh" << std::endl;
            return true;
        }
        
        const char* ptr = data.data();
        const char* end = data.data() + data.size();
        
        auto readU32 = [&]() -> uint32_t {
            if (ptr + 4 > end) throw std::runtime_error("Buffer underflow");
            uint32_t val = *reinterpret_cast<const uint32_t*>(ptr);
            ptr += 4;
            return val;
        };
        auto readU64 = [&]() -> uint64_t {
            if (ptr + 8 > end) throw std::runtime_error("Buffer underflow");
            uint64_t val = *reinterpret_cast<const uint64_t*>(ptr);
            ptr += 8;
            return val;
        };
        auto readString = [&]() -> std::string {
            uint32_t len = readU32();
            if (ptr + len > end) throw std::runtime_error("Buffer underflow");
            std::string s(ptr, len);
            ptr += len;
            return s;
        };
        auto readVecU32 = [&]() -> std::vector<uint32_t> {
            uint32_t size = readU32();
            std::vector<uint32_t> vec;
            if (size > 0) {
                if (ptr + size * 4 > end) throw std::runtime_error("Buffer underflow");
                vec.resize(size);
                memcpy(vec.data(), ptr, size * 4);
                ptr += size * 4;
            }
            return vec;
        };
        auto readVecString = [&]() -> std::vector<std::string> {
            uint32_t size = readU32();
            std::vector<std::string> vec;
            for(uint32_t i=0; i<size; ++i) vec.push_back(readString());
            return vec;
        };

        // 1. Counters
        nextPaperId_ = readU32();
        nextReviewId_ = readU32();
        
        // 2. Papers
        uint32_t paperCount = readU32();
        for(uint32_t i=0; i<paperCount; ++i) {
            Paper p;
            p.paperId = readU32();
            p.title = readString();
            p.abstract = readString();
            p.authorIds = readVecU32();
            p.filepath = readString();
            if (ptr >= end) throw std::runtime_error("Buffer underflow");
            p.status = static_cast<PaperStatus>(*ptr++);
            p.submissionTime = static_cast<time_t>(readU64());
            p.assignedReviewers = readVecU32();
            p.currentVersion = readU32();
            p.revisionPaths = readVecString();
            papers_[p.paperId] = p;
        }
        
        // 3. Reviews
        uint32_t reviewCount = readU32();
        for(uint32_t i=0; i<reviewCount; ++i) {
            Review r;
            r.reviewId = readU32();
            r.paperId = readU32();
            r.reviewerId = readU32();
            if (ptr >= end) throw std::runtime_error("Buffer underflow");
            r.decision = static_cast<ReviewDecision>(*ptr++);
            r.comments = readString();
            r.confidenceScore = readU32();
            r.submitTime = static_cast<time_t>(readU64());
            r.filepath = readString();
            reviews_[r.reviewId] = r;
        }
        
        std::cout << "✅ Metadata loaded (Papers: " << papers_.size() 
                  << ", Reviews: " << reviews_.size() << ")" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to load metadata: " << e.what() << std::endl;
        return true;  // 返回 true 以便系统继续运行
    }
}