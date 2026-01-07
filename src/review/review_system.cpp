#include "review/review_system.h"
#include "user/user.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <random>
#include <set>
#include <unordered_set>
#include <unistd.h> // for getpid()

ReviewSystem::ReviewSystem(std::shared_ptr<Filesystem> fs, 
                          std::shared_ptr<UserManager> userMgr)
    : filesystem_(fs)
    , userManager_(userMgr)
    , nextPaperId_(1)
    , nextReviewId_(1) {
}

bool ReviewSystem::init() {
    return loadMetadata();
}

ReviewSystem::~ReviewSystem() {
    saveMetadataNoLock();
}

// ============================================================================
// 论文提交
// ============================================================================
uint32_t ReviewSystem::submitPaper(uint32_t authorId, 
                                  const std::string& title,
                                  const std::string& abstract,
                                  const std::vector<char>& fileData,
                                  const std::vector<std::string>& keywords) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 验证用户存在
    User user;
    if (!userManager_->getUserById(authorId, user)) {
        std::cerr << "❌ Invalid user ID: " << authorId << std::endl;
        return 0;
    }
    
    // 获取 FileOps
    auto fileOps = filesystem_->getFileOps();

    // 查找可用的 Paper ID
    std::string potentialPath;
    while (true) {
        potentialPath = generatePaperPath(nextPaperId_, 1);
        // 检查文件是否存在 (使用 authorId 权限检查，或者使用 root/admin 权限如果需要)
        // 这里假设 authorId 有权读取/检查该路径，或者 fileExists 不严格检查权限
        if (!fileOps->fileExists(authorId, potentialPath)) {
            break;
        }
        std::cout << "⚠️ Paper ID " << nextPaperId_ << " conflict (file exists: " << potentialPath << "), skipping..." << std::endl;
        nextPaperId_++;
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
    paper.keywords = keywords;
    
    // 生成文件路径
    paper.filepath = potentialPath;
    
    // 保存文件到文件系统 (使用 FileOps 直接操作以设置 Owner)
    // auto fileOps = filesystem_->getFileOps(); // Moved up
    
    // 1. 创建文件 (Owner = authorId)
    // 确保父目录存在 (createFile 内部会检查，但这里我们假设目录结构已由 Admin 初始化)
    if (!fileOps->createFile(authorId, paper.filepath)) {
         std::cerr << "❌ Failed to create paper file: " << paper.filepath << std::endl;
         nextPaperId_--;
         return 0;
    }
    
    // 2. 写入内容
    if (fileOps->writeFile(authorId, paper.filepath, fileData.data(), fileData.size()) != (ssize_t)fileData.size()) {
        std::cerr << "❌ Failed to write paper file content: " << paper.filepath << std::endl;
        nextPaperId_--;
        return 0;
    }
    
    // 保存论文元数据
    papers_[paper.paperId] = paper;
    saveMetadataNoLock();
    
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

// ============================================================================
// 修改论文文件 (覆盖)
// ============================================================================
bool ReviewSystem::updatePaperFile(uint32_t paperId, uint32_t authorId, 
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

    // 只能在 SUBMITTED 状态下修改
    if (paper.status != PaperStatus::SUBMITTED) {
        std::cerr << "❌ Paper can only be modified when status is SUBMITTED" << std::endl;
        return false;
    }
    
    // 获取 FileOps
    auto fileOps = filesystem_->getFileOps();
    
    // 使用临时文件 + 原子替换来保证写入的原子性和可回滚性
    // tmp 名称需要保证文件名长度不超过 Config::MAX_FILENAME - 1
    size_t slash = paper.filepath.find_last_of('/');
    std::string parent = (slash == 0) ? "/" : paper.filepath.substr(0, slash);
    std::string base = paper.filepath.substr(slash + 1);
    std::string suffix = std::string(".tmp.") + std::to_string(getpid());
    size_t maxName = Config::MAX_FILENAME - 1; // reserve space
    if (base.size() + suffix.size() > maxName) {
        base = base.substr(0, maxName - suffix.size());
    }
    std::string tmpName = parent + "/" + base + suffix;

    if (!fileOps->createFile(authorId, tmpName)) {
        std::cerr << "❌ Failed to create tmp file: " << tmpName << std::endl;
        return false;
    }

    ssize_t written = fileOps->writeFile(authorId, tmpName, fileData.data(), fileData.size());
    if (written != (ssize_t)fileData.size()) {
        std::cerr << "❌ Failed to write tmp file content: " << tmpName << " (written=" << written << ", expected=" << fileData.size() << ")" << std::endl;
        fileOps->deleteFile(authorId, tmpName);
        return false;
    }

    // 诊断：打印 tmp 文件信息与 BlockManager 状态
    {
        Inode inode;
        if (filesystem_->getFileOps()->getFileInfo(authorId, tmpName, inode)) {
            std::cerr << "DEBUG: tmp Inode uid=" << inode.uid << " size=" << inode.size << " flags=" << inode.flags << std::endl;
        }
        if (filesystem_->getBlockManager()) filesystem_->getBlockManager()->printStats();
    }

    // 原子替换
    if (!fileOps->atomicReplaceFile(authorId, paper.filepath, tmpName)) {
        std::cerr << "❌ atomicReplaceFile failed for " << paper.filepath << " using " << tmpName << std::endl;
        // 尝试清理 tmp
        fileOps->deleteFile(authorId, tmpName);
        return false;
    }

    std::cout << "✅ Paper file replaced atomically: " << paper.filepath << std::endl;
    return true;
}

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
    // 修复 Bug：将旧文件路径保存到 revisionPaths，并将 filepath 更新为新路径
    paper.revisionPaths.push_back(paper.filepath);
    
    // 保存新版本 (使用 FileOps 直接操作以设置 Owner)
    auto fileOps = filesystem_->getFileOps();
    
    // 1. 创建文件 (Owner = authorId)
    if (!fileOps->createFile(authorId, revisionPath)) {
        std::cerr << "❌ Failed to create revision file: " << revisionPath << std::endl;
        paper.currentVersion--;
        paper.revisionPaths.pop_back();
        return false;
    }
    
    // 2. 写入内容
    if (fileOps->writeFile(authorId, revisionPath, fileData.data(), fileData.size()) != (ssize_t)fileData.size()) {
        std::cerr << "❌ Failed to write revision content: " << revisionPath << std::endl;
        paper.currentVersion--;
        paper.revisionPaths.pop_back();
        return false;
    }
    
    // 更新主文件路径指向新版本
    paper.filepath = revisionPath;
    
    // 如果是上传了修订版，通常意味着之前的版本被拒绝或需要修改
    // 因此需要重置状态为 SUBMITTED 或 UNDER_REVIEW，以便重新分配或重新审稿
    // 这里我们将其重置为 SUBMITTED，等待编辑重新分配或处理
    if (paper.status == PaperStatus::REJECTED) {
       paper.status = PaperStatus::SUBMITTED;
       // 清空分配的审稿人，因为这是新版本，或者是让编辑重新分配？ 
       // 通常流程是：编辑收到Revision -> 重新分配原审稿人或新审稿人
       // 简单起见，重置为 SUBMITTED 状态即可，保留历史审稿记录（在 Reviews 中）
       // 但需要清除 `assignedReviewers` 吗？ 
       // 如果清除，编辑需要重新分配。如果不清除，之前的审稿人还能看到。
       // 策略：重置为 SUBMITTED，保留 assignedReviewers，但因为状态变了，审稿人可能无法立即提交Review（如果只能在UNDER_REVIEW状态提交）
       // 真正完善的流程比较复杂，这里至少先改为 SUBMITTED 让流程能继续
    }

    saveMetadataNoLock();
    
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
        
        // 锁定论文，防止作者修改 (使用 Admin 权限操作)
        filesystem_->getFileOps()->setFileLock(1, paper.filepath, true);
    }
    
    // 授予审稿人 ACL 权限 (只读)
    filesystem_->getFileOps()->grantPermission(1, paper.filepath, reviewerId);
    
    saveMetadataNoLock();
    
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
    saveMetadataNoLock();
    
    return true;
}

// ============================================================================
// 自动分配审稿人
// ============================================================================
bool ReviewSystem::autoAssignReviewers(uint32_t paperId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = papers_.find(paperId);
    if (it == papers_.end()) {
        std::cerr << "❌ Paper not found: " << paperId << std::endl;
        return false;
    }
    Paper& paper = it->second;
    
    // 1. 获取所有潜在审稿人
    std::vector<User> allUsers = userManager_->listAllUsers();
    std::vector<User> candidates;
    
    // 获取作者信息以检查利益冲突
    std::unordered_set<std::string> authorInstitutions;
    for (uint32_t authorId : paper.authorIds) {
        User author;
        if (userManager_->getUserById(authorId, author)) {
            if (!author.institution.empty()) {
                authorInstitutions.insert(author.institution);
            }
        }
    }
    
    // 2. 硬约束筛选 (Hard Constraints)
    for (const auto& user : allUsers) {
        // 必须是审稿人
        if (user.role != UserRole::REVIEWER) continue;
        
        // 不能是作者
        bool isAuthor = false;
        for (uint32_t aid : paper.authorIds) {
            if (user.userId == aid) {
                isAuthor = true;
                break;
            }
        }
        if (isAuthor) continue;
        
        // 利益冲突：同单位
        if (!user.institution.empty() && authorInstitutions.count(user.institution)) {
            continue;
        }
        
        // 负载限制
        // 计算该审稿人当前已分配的论文数
        int currentLoad = 0;
        for (const auto& pPair : papers_) {
            const auto& p = pPair.second;
            if (std::find(p.assignedReviewers.begin(), p.assignedReviewers.end(), user.userId) != p.assignedReviewers.end()) {
                currentLoad++;
            }
        }
        if (currentLoad >= user.maxLoad) {
            continue;
        }
        
        // 已经分配给该论文的不用再分
        if (std::find(paper.assignedReviewers.begin(), paper.assignedReviewers.end(), user.userId) != paper.assignedReviewers.end()) {
            continue;
        }
        
        candidates.push_back(user);
    }
    
    if (candidates.empty()) {
        std::cerr << "⚠️  No eligible reviewers found for paper " << paperId << std::endl;
        return false;
    }
    
    // 3. 软约束评分 (Soft Constraints)
    struct CandidateScore {
        uint32_t userId;
        double score;
    };
    std::vector<CandidateScore> scores;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0); // 随机扰动 0-1 分
    
    for (const auto& candidate : candidates) {
        double score = 0.0;
        
        // 领域匹配 (Jaccard 类似思路，这里简化为交集计数)
        int matchCount = 0;
        for (const auto& pKw : paper.keywords) {
            for (const auto& uInt : candidate.researchInterests) {
                if (pKw == uInt) { // 简单字符串匹配
                    matchCount++;
                }
            }
        }
        score += matchCount * 10.0; // 每个匹配关键词加 10 分
        
        // 随机扰动
        score += dis(gen);
        
        scores.push_back({candidate.userId, score});
    }
    
    // 4. 排序并选择 Top N
    std::sort(scores.begin(), scores.end(), [](const CandidateScore& a, const CandidateScore& b) {
        return a.score > b.score;
    });
    
    int needed = 3 - paper.assignedReviewers.size();
    if (needed <= 0) return true;
    
    int assignedCount = 0;
    for (int i = 0; i < std::min((int)scores.size(), needed); ++i) {
        uint32_t reviewerId = scores[i].userId;
        
        // 执行分配逻辑
        paper.assignedReviewers.push_back(reviewerId);
        
        // 授予权限
        filesystem_->getFileOps()->grantPermission(1, paper.filepath, reviewerId);
        
        assignedCount++;
        std::cout << "✅ Auto-assigned reviewer " << reviewerId << " to paper " << paperId << " (Score: " << scores[i].score << ")" << std::endl;
    }
    
    if (assignedCount > 0 && paper.status == PaperStatus::SUBMITTED) {
        paper.status = PaperStatus::UNDER_REVIEW;
        filesystem_->getFileOps()->setFileLock(1, paper.filepath, true);
    }
    
    return assignedCount > 0;
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
    
    std::cout << "🔍 Checking papers for reviewer " << reviewerId << std::endl;
    
    for (const auto& [paperId, paper] : papers_) {
        // Debug log
        // std::cout << "   Checking paper " << paperId << " (Assigned count: " << paper.assignedReviewers.size() << ")" << std::endl;

        // 检查是否分配给该审稿人
        bool isAssigned = std::find(paper.assignedReviewers.begin(), 
                                    paper.assignedReviewers.end(), 
                                    reviewerId) != paper.assignedReviewers.end();
        
        if (!isAssigned) continue;
        
        std::cout << "   Found assigned paper " << paperId << std::endl;
        
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
            std::cout << "   -> Added to pending list" << std::endl;
            result.push_back(paper);
        } else {
            std::cout << "   -> Already reviewed" << std::endl;
        }
    }
    
    std::cout << "   Total pending papers: " << result.size() << std::endl;
    return result;
}

// ============================================================================
// 🔧 修复问题2 & 问题4：提交评审并自动更新状态
// ============================================================================
uint32_t ReviewSystem::submitReview(uint32_t reviewerId, uint32_t paperId,
                                   ReviewDecision decision, int confidence,
                                   const std::string& comments,
                                   const std::vector<char>& fileData,
                                   const std::string& filename) {
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
    
    // Determine extension
    std::string extension = ".txt";
    if (!filename.empty()) {
        size_t dotPos = filename.find_last_of('.');
        if (dotPos != std::string::npos) {
            extension = filename.substr(dotPos);
        }
    } else if (!fileData.empty()) {
        // Try to guess from magic bytes
        if (fileData.size() > 4 && fileData[0] == '%' && fileData[1] == 'P' && fileData[2] == 'D' && fileData[3] == 'F') {
            extension = ".pdf";
        } else if (fileData.size() > 2 && fileData[0] == 'P' && fileData[1] == 'K') {
            extension = ".docx"; // Assume docx for zip, or could be .zip
        }
    }
    
    std::cout << "   ReviewSystem: Filename='" << filename << "', Extension='" << extension << "'" << std::endl;
    
    review.filepath = generateReviewPath(review.reviewId, extension);
    
    // 保存评审文件
    // 如果有上传的文件数据，优先保存文件数据
    if (!fileData.empty()) {
        if (!filesystem_->writeFile(review.filepath, fileData)) {
            std::cerr << "❌ Failed to write review file: " << review.filepath << std::endl;
            nextReviewId_--;
            return 0;
        }
    } else {
        // 否则保存评论文本
        std::vector<char> reviewData(comments.begin(), comments.end());
        if (!filesystem_->writeFile(review.filepath, reviewData)) {
            std::cerr << "❌ Failed to write review file: " << review.filepath << std::endl;
            nextReviewId_--;
            return 0;
        }
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
    
    saveMetadataNoLock();
    
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
    saveMetadataNoLock();
    
    std::cout << "✅ Final decision made!" << std::endl;
    std::cout << "   Paper ID: " << paperId << std::endl;
    std::cout << "   Decision: " << it->second.getStatusName() << std::endl;
    
    return true;
}

// ============================================================================
// 获取审稿人的所有评审记录
// ============================================================================
std::vector<Review> ReviewSystem::getReviewsByReviewer(uint32_t reviewerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Review> result;
    
    for (const auto& pair : reviews_) {
        if (pair.second.reviewerId == reviewerId) {
            result.push_back(pair.second);
        }
    }
    return result;
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

uint32_t ReviewSystem::getReviewCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return reviews_.size();
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

std::string ReviewSystem::generateReviewPath(uint32_t reviewId, const std::string& extension) {
    std::ostringstream oss;
    oss << "/reviews/review_" << std::setw(6) << std::setfill('0') << reviewId << extension;
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
    return saveMetadataNoLock();
}

bool ReviewSystem::saveMetadataNoLock() {
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
            writeVecString(p.keywords); // New field
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

        // Clear existing data before loading
        papers_.clear();
        reviews_.clear();
        
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
        // Only update counters if they are larger than current (to avoid regression on partial load)
        // But since we cleared papers_, we should probably trust the file.
        // However, if we are reloading, we might want to be careful.
        // For now, let's trust the file.
        uint32_t loadedNextPaperId = readU32();
        uint32_t loadedNextReviewId = readU32();
        
        if (loadedNextPaperId > nextPaperId_) nextPaperId_ = loadedNextPaperId;
        if (loadedNextReviewId > nextReviewId_) nextReviewId_ = loadedNextReviewId;
        
        // 2. Papers
        uint32_t paperCount = readU32();
        for(uint32_t i=0; i<paperCount; ++i) {
            Paper p;
            p.paperId = readU32();
            if (p.paperId == 0 || p.paperId > 1000000) {
                throw std::runtime_error("Corrupted metadata: Invalid Paper ID detected");
            }

            p.title = readString();
            p.abstract = readString();
            p.authorIds = readVecU32();
            p.filepath = readString();
            if (ptr >= end) throw std::runtime_error("Buffer underflow");
            
            uint8_t statusByte = static_cast<uint8_t>(*ptr++);
            if (statusByte > 10) { // Assuming reasonable max status
                 throw std::runtime_error("Corrupted metadata: Invalid Status");
            }
            p.status = static_cast<PaperStatus>(statusByte);

            p.submissionTime = static_cast<time_t>(readU64());
            p.assignedReviewers = readVecU32();
            p.keywords = readVecString(); // New field
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
        std::cerr << "CRITICAL ERROR: Metadata corruption detected. System starting with empty state." << std::endl;
        // Optionally: print data size
        // std::cerr << "Read " << data.size() << " bytes." << std::endl;
        return true;  // Keep running empty, but log error
    }
}