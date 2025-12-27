#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <ctime>

enum class PaperStatus : uint8_t {
    SUBMITTED = 0,      // 已提交
    UNDER_REVIEW = 1,   // 审稿中
    REVIEWED = 2,       // 已审稿
    ACCEPTED = 3,       // 已接收
    REJECTED = 4        // 已拒绝
};

struct Paper {
    uint32_t paperId;
    std::string title;
    std::string abstract;
    std::vector<uint32_t> authorIds;    // 作者ID列表
    std::string filepath;               // 论文文件在文件系统中的路径
    PaperStatus status;
    time_t submissionTime;
    std::vector<uint32_t> assignedReviewers;  // 分配的审稿人ID
    
    // 版本控制
    uint32_t currentVersion;
    std::vector<std::string> revisionPaths;   // 修订版本路径列表
    
    Paper() : paperId(0), status(PaperStatus::SUBMITTED), 
              submissionTime(0), currentVersion(1) {}
    
    std::string getStatusName() const {
        switch (status) {
            case PaperStatus::SUBMITTED: return "Submitted";
            case PaperStatus::UNDER_REVIEW: return "Under Review";
            case PaperStatus::REVIEWED: return "Reviewed";
            case PaperStatus::ACCEPTED: return "Accepted";
            case PaperStatus::REJECTED: return "Rejected";
            default: return "Unknown";
        }
    }
};
