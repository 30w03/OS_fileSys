#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <ctime>

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
        // 修改论文 (Overwrite)
        MSG_UPDATE_PAPER_FILE_REQUEST = 120,
        MSG_UPDATE_PAPER_FILE_RESPONSE = 121,
        
        // 审稿人分配 (编辑操作)
        MSG_GET_ALL_PAPERS_REQUEST = 70,
        MSG_GET_ALL_PAPERS_RESPONSE = 71,
        MSG_ASSIGN_REVIEWER_REQUEST = 72,
        MSG_ASSIGN_REVIEWER_RESPONSE = 73,
        MSG_REMOVE_REVIEWER_REQUEST = 74,
        MSG_REMOVE_REVIEWER_RESPONSE = 75,
        MSG_AUTO_ASSIGN_REQUEST = 76,
        MSG_AUTO_ASSIGN_RESPONSE = 77,
        
        // 评审操作
        MSG_GET_PAPERS_TO_REVIEW_REQUEST = 80,
        MSG_GET_PAPERS_TO_REVIEW_RESPONSE = 81,
        MSG_SUBMIT_REVIEW_REQUEST = 82,
        MSG_SUBMIT_REVIEW_RESPONSE = 83,
        MSG_GET_REVIEWS_REQUEST = 84,
        MSG_GET_REVIEWS_RESPONSE = 85,
        MSG_GET_REVIEWER_HISTORY_REQUEST = 86, // 🔥 New
        MSG_GET_REVIEWER_HISTORY_RESPONSE = 87, // 🔥 New

        // 用户资料
        MSG_UPDATE_PROFILE_REQUEST = 90,
        MSG_UPDATE_PROFILE_RESPONSE = 91,

        
        // 编辑决定
        MSG_MAKE_DECISION_REQUEST = 95,
        MSG_MAKE_DECISION_RESPONSE = 96,
        
        // 统计信息
        MSG_GET_STATISTICS_REQUEST = 100,
        MSG_GET_STATISTICS_RESPONSE = 101,
        
        // 系统监控
        MSG_GET_SYSTEM_STATS_REQUEST = 102,
        MSG_GET_SYSTEM_STATS_RESPONSE = 103,
        MSG_LIST_ONLINE_USERS_REQUEST = 104,
        MSG_LIST_ONLINE_USERS_RESPONSE = 105,
        
        // 用户管理 (管理员)
        MSG_UPDATE_USER_ROLE_REQUEST = 110,
        MSG_UPDATE_USER_ROLE_RESPONSE = 111,
        MSG_DEACTIVATE_USER_REQUEST = 112,
        MSG_DEACTIVATE_USER_RESPONSE = 113,
        MSG_SYSTEM_BACKUP_REQUEST = 114,
        MSG_SYSTEM_BACKUP_RESPONSE = 115
    };
    
    struct MessageHeader {
        uint32_t magic;        // 0x50525346 ("PRSF")
        uint8_t version;       // 1
        uint8_t type;          // MessageType
        uint16_t reserved;     // Padding
        uint32_t length;       // Payload length
        uint64_t timestamp;    // Timestamp
        uint32_t checksum;     // CRC32
        
        MessageHeader() 
            : magic(0x50525346), version(1), type(0), reserved(0), 
              length(0), timestamp(static_cast<uint64_t>(std::time(nullptr))), checksum(0) {}
              
        static constexpr size_t SIZE = 24;
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
                                           const std::string& abstract, const std::vector<char>& fileData,
                                           const std::vector<std::string>& keywords = {});
    static Message createSubmitPaperResponse(bool success, uint32_t paperId, const std::string& message);
    
    // 🔥 New: Update Paper File
    static Message createUpdatePaperFileRequest(uint32_t sessionId, uint32_t paperId, const std::vector<char>& fileData);
    static Message createUpdatePaperFileResponse(bool success, const std::string& message);
    static bool parseUpdatePaperFileRequest(const Message& msg, uint32_t& sessionId, uint32_t& paperId, std::vector<char>& fileData);
    static bool parseUpdatePaperFileResponse(const Message& msg, bool& success, std::string& message);

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
    
    // ============ 系统监控 ============
    static Message createGetSystemStatsRequest(uint32_t sessionId);
    static Message createGetSystemStatsResponse(bool success, const std::string& stats);
    static Message createListOnlineUsersRequest(uint32_t sessionId);
    static Message createListOnlineUsersResponse(bool success, const std::string& userList);
    
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
                                       std::string& abstract, std::vector<char>& fileData,
                                       std::vector<std::string>& keywords);
    static bool parseSubmitPaperResponse(const Message& msg, bool& success, uint32_t& paperId, std::string& message);
    
    static bool parseGetMyPapersRequest(const Message& msg, uint32_t& sessionId);
    static bool parseGetMyPapersResponse(const Message& msg, bool& success, std::vector<PaperInfo>& papers);
    
    static bool parseGetPapersToReviewRequest(const Message& msg, uint32_t& sessionId);
    static bool parseGetPapersToReviewResponse(const Message& msg, bool& success, std::vector<PaperInfo>& papers);
    
    static bool parseSubmitReviewRequest(const Message& msg, uint32_t& sessionId, uint32_t& paperId,
                                        std::string& decision, int& confidence, std::string& comments);
    static bool parseSubmitReviewResponse(const Message& msg, bool& success, uint32_t& reviewId, std::string& message);
    
    static bool parseAssignReviewerRequest(const Message& msg, uint32_t& sessionId, uint32_t& paperId, uint32_t& reviewerId);
    
    // 自动分配
    static Message createAutoAssignRequest(uint32_t sessionId, uint32_t paperId);
    static bool parseAutoAssignRequest(const Message& msg, uint32_t& sessionId, uint32_t& paperId);
    static Message createAutoAssignResponse(bool success, const std::string& message);
    static bool parseAutoAssignResponse(const Message& msg, bool& success, std::string& message);

    // 更新资料
    static Message createUpdateProfileRequest(uint32_t sessionId, const std::string& institution, 
                                            const std::vector<std::string>& interests, int maxLoad);
    static bool parseUpdateProfileRequest(const Message& msg, uint32_t& sessionId, 
                                        std::string& institution, std::vector<std::string>& interests, int& maxLoad);
    static Message createUpdateProfileResponse(bool success, const std::string& message);
    static bool parseUpdateProfileResponse(const Message& msg, bool& success, std::string& message);

    static bool parseGetAllPapersRequest(const Message& msg, uint32_t& sessionId);
    static bool parseGetAllPapersResponse(const Message& msg, bool& success, std::vector<PaperInfo>& papers);
    
    // 🔥 新增：解析获取评审响应
    static bool parseGetReviewsResponse(const Message& msg, bool& success, std::vector<ReviewInfo>& reviews);
    
    // 🔥 新增：获取审稿历史
    static Message createGetReviewerHistoryRequest(uint32_t sessionId);
    static bool parseGetReviewerHistoryRequest(const Message& msg, uint32_t& sessionId);
    static Message createGetReviewerHistoryResponse(bool success, const std::vector<ReviewInfo>& reviews);
    static bool parseGetReviewerHistoryResponse(const Message& msg, bool& success, std::vector<ReviewInfo>& reviews);

    // ============ 用户管理 (管理员) ============
    static Message createUpdateUserRoleRequest(uint32_t sessionId, uint32_t userId, const std::string& role);
    static bool parseUpdateUserRoleRequest(const Message& msg, uint32_t& sessionId, uint32_t& userId, std::string& role);
    static Message createUpdateUserRoleResponse(bool success, const std::string& message);
    static bool parseUpdateUserRoleResponse(const Message& msg, bool& success, std::string& message);
    
    static Message createDeactivateUserRequest(uint32_t sessionId, uint32_t userId);
    static bool parseDeactivateUserRequest(const Message& msg, uint32_t& sessionId, uint32_t& userId);
    static Message createDeactivateUserResponse(bool success, const std::string& message);
    static bool parseDeactivateUserResponse(const Message& msg, bool& success, std::string& message);
    
    static Message createSystemBackupRequest(uint32_t sessionId);
    static bool parseSystemBackupRequest(const Message& msg, uint32_t& sessionId);
    static Message createSystemBackupResponse(bool success, const std::string& message);
    static bool parseSystemBackupResponse(const Message& msg, bool& success, std::string& message);
    
    // 补充缺失的解析函数声明
    static bool parseUploadRevisionRequest(const Message& msg, uint32_t& sessionId, uint32_t& paperId, std::vector<char>& fileData);
    static bool parseUploadRevisionResponse(const Message& msg, bool& success, std::string& message);
    
    static bool parseDownloadPaperRequest(const Message& msg, uint32_t& sessionId, uint32_t& paperId);
    static bool parseDownloadPaperResponse(const Message& msg, bool& success, std::vector<char>& data);
    
    static bool parseMakeDecisionRequest(const Message& msg, uint32_t& sessionId, uint32_t& paperId, std::string& decision);
    static bool parseMakeDecisionResponse(const Message& msg, bool& success, std::string& message);
};