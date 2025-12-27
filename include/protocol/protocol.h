#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct FileListEntry {
    std::string filename;
    uint64_t size;
    uint64_t timestamp;
};

// 🔥 评审信息结构（前置声明，移到 PaperInfo 之前）
struct ReviewInfo {
    uint32_t reviewId;
    uint32_t paperId;
    uint32_t reviewerId;
    std::string reviewerName;     // 🔥 新增：审稿人用户名
    std::string decision;
    int confidenceScore;
    std::string comments;          // 🔥 新增：评审意见
    uint64_t submitTime;
};

// 论文信息结构
struct PaperInfo {
    uint32_t paperId;
    std::string title;
    std::string abstract;
    std::string status;
    uint64_t submissionTime;
    uint32_t currentVersion;
    std::vector<uint32_t> authorIds;
    std::vector<uint32_t> reviewerIds;
    std::vector<ReviewInfo> reviews;  // 🔥 评审列表
};

// 🔥 统计信息结构
struct Statistics {
    uint32_t total_papers;
    uint32_t total_reviews;
    uint32_t total_users;
    uint32_t pending_papers;
    uint32_t accepted_papers;
    uint32_t rejected_papers;
};

class Protocol {
public:
    enum MessageType : uint8_t {
        // 基础消息
        MSG_PING = 1,
        MSG_PONG = 2,
        
        // 文件操作
        MSG_FILE_LIST_REQUEST = 10,
        MSG_FILE_LIST_RESPONSE = 11,
        MSG_FILE_UPLOAD_REQUEST = 20,
        MSG_FILE_UPLOAD_RESPONSE = 21,
        MSG_FILE_DOWNLOAD_REQUEST = 30,
        MSG_FILE_DOWNLOAD_RESPONSE = 31,
        MSG_FILE_DELETE_REQUEST = 40,
        MSG_FILE_DELETE_RESPONSE = 41,
        
        // 用户认证
        MSG_LOGIN_REQUEST = 50,
        MSG_LOGIN_RESPONSE = 51,
        MSG_LOGOUT_REQUEST = 52,
        MSG_LOGOUT_RESPONSE = 53,
        MSG_REGISTER_REQUEST = 54,
        MSG_REGISTER_RESPONSE = 55,
        
        // 论文提交
        MSG_SUBMIT_PAPER_REQUEST = 60,
        MSG_SUBMIT_PAPER_RESPONSE = 61,
        MSG_UPLOAD_REVISION_REQUEST = 62,
        MSG_UPLOAD_REVISION_RESPONSE = 63,
        MSG_GET_MY_PAPERS_REQUEST = 64,
        MSG_GET_MY_PAPERS_RESPONSE = 65,
        MSG_GET_PAPER_INFO_REQUEST = 66,
        MSG_GET_PAPER_INFO_RESPONSE = 67,
        MSG_DOWNLOAD_PAPER_REQUEST = 68,
        MSG_DOWNLOAD_PAPER_RESPONSE = 69,
        
        // 审稿人分配 (编辑操作)
        MSG_GET_ALL_PAPERS_REQUEST = 70,
        MSG_GET_ALL_PAPERS_RESPONSE = 71,
        MSG_ASSIGN_REVIEWER_REQUEST = 72,
        MSG_ASSIGN_REVIEWER_RESPONSE = 73,
        MSG_REMOVE_REVIEWER_REQUEST = 74,
        MSG_REMOVE_REVIEWER_RESPONSE = 75,
        
        // 评审操作
        MSG_GET_PAPERS_TO_REVIEW_REQUEST = 80,
        MSG_GET_PAPERS_TO_REVIEW_RESPONSE = 81,
        MSG_SUBMIT_REVIEW_REQUEST = 82,
        MSG_SUBMIT_REVIEW_RESPONSE = 83,
        MSG_GET_REVIEWS_REQUEST = 84,
        MSG_GET_REVIEWS_RESPONSE = 85,
        
        // 编辑决定
        MSG_MAKE_DECISION_REQUEST = 90,
        MSG_MAKE_DECISION_RESPONSE = 91,
        
        // 统计信息
        MSG_GET_STATISTICS_REQUEST = 100,
        MSG_GET_STATISTICS_RESPONSE = 101
    };
    
    struct MessageHeader {
        uint32_t magic;
        MessageType type;
        uint32_t payloadSize;
        uint32_t checksum;
        
        MessageHeader() : magic(0x12345678), type(MSG_PING), payloadSize(0), checksum(0) {}
    };
    
    struct Message {
        MessageHeader header;
        std::vector<char> payload;

        Message() = default;
    };
    
    // 校验和计算
    static uint32_t calculateChecksum(const std::vector<char>& data);
    
    // ============ 基础消息 ============
    static Message createPingMessage();
    static Message createPongMessage();
    
    // ============ 文件操作 ============
    static Message createFileListRequest();
    static Message createFileListResponse(const std::vector<FileListEntry>& entries);
    
    static Message createFileUploadRequest(const std::string& remotePath, const std::vector<char>& data);
    static Message createFileUploadResponse(bool success);
    
    static Message createFileDownloadRequest(const std::string& remotePath);
    static Message createFileDownloadResponse(bool success, const std::vector<char>& data);
    
    static Message createFileDeleteRequest(const std::string& remotePath);
    static Message createFileDeleteResponse(bool success);
    
    // ============ 用户认证 ============
    static Message createLoginRequest(const std::string& username, const std::string& password);
    static Message createLoginResponse(bool success, uint32_t sessionId, uint32_t userId, const std::string& role);
    
    static Message createLogoutRequest(uint32_t sessionId);
    static Message createLogoutResponse(bool success);
    
    static Message createRegisterRequest(const std::string& username, const std::string& password, const std::string& role);
    static Message createRegisterResponse(bool success, const std::string& message);
    
    // ============ 论文提交 ============
    static Message createSubmitPaperRequest(uint32_t sessionId, const std::string& title, 
                                           const std::string& abstract, const std::vector<char>& fileData);
    static Message createSubmitPaperResponse(bool success, uint32_t paperId, const std::string& message);
    
    static Message createUploadRevisionRequest(uint32_t sessionId, uint32_t paperId, const std::vector<char>& fileData);
    static Message createUploadRevisionResponse(bool success, const std::string& message);
    
    static Message createGetMyPapersRequest(uint32_t sessionId);
    static Message createGetMyPapersResponse(bool success, const std::vector<PaperInfo>& papers);
    
    static Message createGetPaperInfoRequest(uint32_t sessionId, uint32_t paperId);
    static Message createGetPaperInfoResponse(bool success, const PaperInfo& paper);
    
    static Message createDownloadPaperRequest(uint32_t sessionId, uint32_t paperId);
    static Message createDownloadPaperResponse(bool success, const std::vector<char>& data);
    
    // ============ 编辑操作 ============
    static Message createGetAllPapersRequest(uint32_t sessionId);
    static Message createGetAllPapersResponse(bool success, const std::vector<PaperInfo>& papers);
    
    static Message createAssignReviewerRequest(uint32_t sessionId, uint32_t paperId, uint32_t reviewerId);
    static Message createAssignReviewerResponse(bool success, const std::string& message);
    
    // ============ 审稿操作 ============
    static Message createGetPapersToReviewRequest(uint32_t sessionId);
    static Message createGetPapersToReviewResponse(bool success, const std::vector<PaperInfo>& papers);
    
    static Message createSubmitReviewRequest(uint32_t sessionId, uint32_t paperId, 
                                            const std::string& decision, int confidence, 
                                            const std::string& comments);
    static Message createSubmitReviewResponse(bool success, uint32_t reviewId, const std::string& message);
    
    static Message createGetReviewsRequest(uint32_t sessionId, uint32_t paperId);
    static Message createGetReviewsResponse(bool success, const std::vector<ReviewInfo>& reviews);
    
    // ============ 编辑决定 ============
    static Message createMakeDecisionRequest(uint32_t sessionId, uint32_t paperId, const std::string& decision);
    static Message createMakeDecisionResponse(bool success, const std::string& message);
    
    // ============ 统计信息 ============
    static Message createGetStatisticsRequest(uint32_t sessionId);
    static Message createGetStatisticsResponse(bool success, const std::string& stats);
    
    // ============ 解析函数 ============
    static bool parseFileListResponse(const Message& msg, std::vector<FileListEntry>& entries);
    static bool parseFileUploadRequest(const Message& msg, std::string& path, std::vector<char>& data);
    static bool parseFileDownloadRequest(const Message& msg, std::string& path);
    static bool parseFileDownloadResponse(const Message& msg, bool& success, std::vector<char>& data);
    static bool parseFileDeleteRequest(const Message& msg, std::string& path);
    
    static bool parseLoginRequest(const Message& msg, std::string& username, std::string& password);
    static bool parseLoginResponse(const Message& msg, bool& success, uint32_t& sessionId, uint32_t& userId, std::string& role);
    
    static bool parseRegisterRequest(const Message& msg, std::string& username, std::string& password, std::string& role);
    static bool parseRegisterResponse(const Message& msg, bool& success, std::string& message);
    
    static bool parseSubmitPaperRequest(const Message& msg, uint32_t& sessionId, std::string& title, 
                                       std::string& abstract, std::vector<char>& fileData);
    static bool parseSubmitPaperResponse(const Message& msg, bool& success, uint32_t& paperId, std::string& message);
    
    static bool parseGetMyPapersRequest(const Message& msg, uint32_t& sessionId);
    static bool parseGetMyPapersResponse(const Message& msg, bool& success, std::vector<PaperInfo>& papers);
    
    static bool parseGetPapersToReviewRequest(const Message& msg, uint32_t& sessionId);
    static bool parseGetPapersToReviewResponse(const Message& msg, bool& success, std::vector<PaperInfo>& papers);
    
    static bool parseSubmitReviewRequest(const Message& msg, uint32_t& sessionId, uint32_t& paperId,
                                        std::string& decision, int& confidence, std::string& comments);
    static bool parseSubmitReviewResponse(const Message& msg, bool& success, uint32_t& reviewId, std::string& message);
    
    static bool parseAssignReviewerRequest(const Message& msg, uint32_t& sessionId, uint32_t& paperId, uint32_t& reviewerId);
    
    static bool parseGetAllPapersRequest(const Message& msg, uint32_t& sessionId);
    static bool parseGetAllPapersResponse(const Message& msg, bool& success, std::vector<PaperInfo>& papers);
    
    // 🔥 新增：解析获取评审响应
    static bool parseGetReviewsResponse(const Message& msg, bool& success, std::vector<ReviewInfo>& reviews);
};