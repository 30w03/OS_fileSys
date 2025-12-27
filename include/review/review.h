#pragma once
#include <string>
#include <cstdint>
#include <ctime>

enum class ReviewDecision : uint8_t {
    STRONG_ACCEPT = 0,
    ACCEPT = 1,
    WEAK_ACCEPT = 2,
    BORDERLINE = 3,
    WEAK_REJECT = 4,
    REJECT = 5,
    STRONG_REJECT = 6
};

struct Review {
    uint32_t reviewId;
    uint32_t paperId;
    uint32_t reviewerId;
    ReviewDecision decision;
    std::string comments;
    int confidenceScore;      // 1-5: 评审人对自己评审的信心
    time_t submitTime;
    std::string filepath;     // 评审文件在文件系统中的路径
    
    Review() : reviewId(0), paperId(0), reviewerId(0), 
               decision(ReviewDecision::BORDERLINE), 
               confidenceScore(3), submitTime(0) {}
    
    std::string getDecisionName() const {
        switch (decision) {
            case ReviewDecision::STRONG_ACCEPT: return "Strong Accept";
            case ReviewDecision::ACCEPT: return "Accept";
            case ReviewDecision::WEAK_ACCEPT: return "Weak Accept";
            case ReviewDecision::BORDERLINE: return "Borderline";
            case ReviewDecision::WEAK_REJECT: return "Weak Reject";
            case ReviewDecision::REJECT: return "Reject";
            case ReviewDecision::STRONG_REJECT: return "Strong Reject";
            default: return "Unknown";
        }
    }
};
