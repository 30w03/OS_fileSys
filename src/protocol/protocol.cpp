#include "protocol/protocol.h"
#include <cstring>
#include <sstream>
#include <arpa/inet.h>

// ============================================================================
// 辅助函数：计算校验和 (CRC32)
// ============================================================================
static uint32_t crc32(const void* data, size_t n_bytes) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < n_bytes; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

uint32_t Protocol::calculateChecksum(const std::vector<char>& data) {
    return crc32(data.data(), data.size());
}

// ============================================================================
// 序列化辅助函数
// ============================================================================

static void writeUint32(std::vector<char>& buffer, uint32_t value) {
    size_t offset = buffer.size();
    buffer.resize(offset + sizeof(uint32_t));
    uint32_t n_value = htonl(value);
    std::memcpy(buffer.data() + offset, &n_value, sizeof(uint32_t));
}

static bool readUint32(const char*& ptr, const char* end, uint32_t& value) {
    if (ptr + sizeof(uint32_t) > end) return false;
    uint32_t n_value;
    std::memcpy(&n_value, ptr, sizeof(uint32_t));
    value = ntohl(n_value);
    ptr += sizeof(uint32_t);
    return true;
}

static void writeString(std::vector<char>& buffer, const std::string& str) {
    uint32_t len = str.length();
    writeUint32(buffer, len);
    
    size_t offset = buffer.size();
    buffer.resize(offset + len);
    std::memcpy(buffer.data() + offset, str.c_str(), len);
}

static bool readString(const char*& ptr, const char* end, std::string& str) {
    uint32_t len;
    if (!readUint32(ptr, end, len)) return false;
    
    if (ptr + len > end) return false;
    
    str.assign(ptr, len);
    ptr += len;
    
    return true;
}

static void writeInt32(std::vector<char>& buffer, int32_t value) {
    writeUint32(buffer, static_cast<uint32_t>(value));
}

static bool readInt32(const char*& ptr, const char* end, int32_t& value) {
    uint32_t u_val;
    if (!readUint32(ptr, end, u_val)) return false;
    value = static_cast<int32_t>(u_val);
    return true;
}

static void writeUint64(std::vector<char>& buffer, uint64_t value) {
    size_t offset = buffer.size();
    buffer.resize(offset + sizeof(uint64_t));
    
    uint32_t high = htonl(static_cast<uint32_t>(value >> 32));
    uint32_t low = htonl(static_cast<uint32_t>(value & 0xFFFFFFFF));
    
    std::memcpy(buffer.data() + offset, &high, 4);
    std::memcpy(buffer.data() + offset + 4, &low, 4);
}

static bool readUint64(const char*& ptr, const char* end, uint64_t& value) {
    if (ptr + sizeof(uint64_t) > end) return false;
    
    uint32_t high, low;
    std::memcpy(&high, ptr, 4);
    std::memcpy(&low, ptr + 4, 4);
    
    value = (static_cast<uint64_t>(ntohl(high)) << 32) | ntohl(low);
    ptr += sizeof(uint64_t);
    return true;
}

static void writeBool(std::vector<char>& buffer, bool value) {
    buffer.push_back(value ? 1 : 0);
}

static bool readBool(const char*& ptr, const char* end, bool& value) {
    if (ptr >= end) return false;
    value = (*ptr != 0);
    ptr++;
    return true;
}

static void writeUint32Vector(std::vector<char>& buffer, const std::vector<uint32_t>& vec) {
    writeUint32(buffer, vec.size());
    for (uint32_t val : vec) {
        writeUint32(buffer, val);
    }
}

static bool readUint32Vector(const char*& ptr, const char* end, std::vector<uint32_t>& vec) {
    uint32_t count;
    if (!readUint32(ptr, end, count)) return false;
    
    vec.clear();
    vec.reserve(count);
    
    for (uint32_t i = 0; i < count; i++) {
        uint32_t val;
        if (!readUint32(ptr, end, val)) return false;
        vec.push_back(val);
    }
    
    return true;
}

// 1️⃣ 先定义 ReviewInfo 的读写函数
static void writeReviewInfo(std::vector<char>& buffer, const ReviewInfo& review) {
    writeUint32(buffer, review.reviewId);
    writeUint32(buffer, review.paperId);
    writeUint32(buffer, review.reviewerId);
    writeString(buffer, review.reviewerName);  // 🔥 添加
    writeString(buffer, review.decision);
    writeInt32(buffer, review.confidenceScore);
    writeString(buffer, review.comments);      // 🔥 添加
    writeUint64(buffer, review.submitTime);
}

static bool readReviewInfo(const char*& ptr, const char* end, ReviewInfo& review) {
    if (!readUint32(ptr, end, review.reviewId)) return false;
    if (!readUint32(ptr, end, review.paperId)) return false;
    if (!readUint32(ptr, end, review.reviewerId)) return false;
    if (!readString(ptr, end, review.reviewerName)) return false;  // 🔥 添加
    if (!readString(ptr, end, review.decision)) return false;
    if (!readInt32(ptr, end, review.confidenceScore)) return false;
    if (!readString(ptr, end, review.comments)) return false;      // 🔥 添加
    if (!readUint64(ptr, end, review.submitTime)) return false;
    return true;
}

// 2️⃣ 再定义 PaperInfo 的读写函数（现在可以调用 ReviewInfo 的函数了）
static void writePaperInfo(std::vector<char>& buffer, const PaperInfo& paper) {
    writeUint32(buffer, paper.paperId);
    writeString(buffer, paper.title);
    writeString(buffer, paper.abstract);
    writeString(buffer, paper.status);
    writeUint64(buffer, paper.submissionTime);
    writeUint32(buffer, paper.currentVersion);
    writeUint32Vector(buffer, paper.authorIds);
    writeUint32Vector(buffer, paper.reviewerIds);
    
    // 🔥 添加：写入评审列表
    writeUint32(buffer, paper.reviews.size());
    for (const auto& review : paper.reviews) {
        writeReviewInfo(buffer, review);
    }
}

static bool readPaperInfo(const char*& ptr, const char* end, PaperInfo& paper) {
    if (!readUint32(ptr, end, paper.paperId)) return false;
    if (!readString(ptr, end, paper.title)) return false;
    if (!readString(ptr, end, paper.abstract)) return false;
    if (!readString(ptr, end, paper.status)) return false;
    if (!readUint64(ptr, end, paper.submissionTime)) return false;
    if (!readUint32(ptr, end, paper.currentVersion)) return false;
    if (!readUint32Vector(ptr, end, paper.authorIds)) return false;
    if (!readUint32Vector(ptr, end, paper.reviewerIds)) return false;
    
    // 🔥 添加：读取评审列表
    uint32_t reviewCount;
    if (!readUint32(ptr, end, reviewCount)) return false;
    paper.reviews.clear();
    paper.reviews.reserve(reviewCount);
    for (uint32_t i = 0; i < reviewCount; i++) {
        ReviewInfo review;
        if (!readReviewInfo(ptr, end, review)) return false;
        paper.reviews.push_back(review);
    }
    
    return true;
}

// ============================================================================
// Ping/Pong 消息
// ============================================================================
Protocol::Message Protocol::createPingMessage() {
    Message msg;
    msg.header.type = MSG_PING;
    msg.header.length = 0;
    msg.header.checksum = 0;
    return msg;
}

Protocol::Message Protocol::createPongMessage() {
    Message msg;
    msg.header.type = MSG_PONG;
    msg.header.length = 0;
    msg.header.checksum = 0;
    return msg;
}

// ============================================================================
// 文件列表消息
// ============================================================================
Protocol::Message Protocol::createFileListRequest() {
    Message msg;
    msg.header.type = MSG_FILE_LIST_REQUEST;
    msg.header.length = 0;
    msg.header.checksum = 0;
    return msg;
}

Protocol::Message Protocol::createFileListResponse(const std::vector<FileListEntry>& entries) {
    Message msg;
    msg.header.type = MSG_FILE_LIST_RESPONSE;
    
    writeUint32(msg.payload, entries.size());
    
    for (const auto& entry : entries) {
        writeString(msg.payload, entry.filename);
        writeUint64(msg.payload, entry.size);
        writeUint64(msg.payload, entry.timestamp);
    }
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

bool Protocol::parseFileListResponse(const Message& msg, std::vector<FileListEntry>& entries) {
    if (msg.header.type != MSG_FILE_LIST_RESPONSE) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    uint32_t count;
    if (!readUint32(ptr, end, count)) return false;
    
    entries.clear();
    entries.reserve(count);
    
    for (uint32_t i = 0; i < count; i++) {
        FileListEntry entry;
        
        if (!readString(ptr, end, entry.filename)) return false;
        if (!readUint64(ptr, end, entry.size)) return false;
        if (!readUint64(ptr, end, entry.timestamp)) return false;
        
        entries.push_back(entry);
    }
    
    return true;
}

// ============================================================================
// 文件上传消息
// ============================================================================
Protocol::Message Protocol::createFileUploadRequest(const std::string& remotePath, 
                                                    const std::vector<char>& data) {
    Message msg;
    msg.header.type = MSG_FILE_UPLOAD_REQUEST;
    
    writeString(msg.payload, remotePath);
    writeUint32(msg.payload, data.size());
    
    size_t offset = msg.payload.size();
    msg.payload.resize(offset + data.size());
    std::memcpy(msg.payload.data() + offset, data.data(), data.size());
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createFileUploadResponse(bool success) {
    Message msg;
    msg.header.type = MSG_FILE_UPLOAD_RESPONSE;
    
    writeBool(msg.payload, success);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

bool Protocol::parseFileUploadRequest(const Message& msg, std::string& path, 
                                      std::vector<char>& data) {
    if (msg.header.type != MSG_FILE_UPLOAD_REQUEST) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    if (!readString(ptr, end, path)) return false;
    
    uint32_t dataSize;
    if (!readUint32(ptr, end, dataSize)) return false;
    
    if (ptr + dataSize > end) return false;
    
    data.assign(ptr, ptr + dataSize);
    
    return true;
}

// ============================================================================
// 文件下载消息
// ============================================================================
Protocol::Message Protocol::createFileDownloadRequest(const std::string& remotePath) {
    Message msg;
    msg.header.type = MSG_FILE_DOWNLOAD_REQUEST;
    
    writeString(msg.payload, remotePath);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createFileDownloadResponse(bool success, 
                                                       const std::vector<char>& data) {
    Message msg;
    msg.header.type = MSG_FILE_DOWNLOAD_RESPONSE;
    
    writeBool(msg.payload, success);
    
    if (success) {
        writeUint32(msg.payload, data.size());
        
        size_t offset = msg.payload.size();
        msg.payload.resize(offset + data.size());
        std::memcpy(msg.payload.data() + offset, data.data(), data.size());
    }
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

bool Protocol::parseFileDownloadRequest(const Message& msg, std::string& path) {
    if (msg.header.type != MSG_FILE_DOWNLOAD_REQUEST) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    return readString(ptr, end, path);
}

bool Protocol::parseFileDownloadResponse(const Message& msg, bool& success, 
                                         std::vector<char>& data) {
    if (msg.header.type != MSG_FILE_DOWNLOAD_RESPONSE) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    if (!readBool(ptr, end, success)) return false;
    
    if (success) {
        uint32_t dataSize;
        if (!readUint32(ptr, end, dataSize)) return false;
        
        if (ptr + dataSize > end) return false;
        
        data.assign(ptr, ptr + dataSize);
    }
    
    return true;
}

// ============================================================================
// 文件删除消息
// ============================================================================
Protocol::Message Protocol::createFileDeleteRequest(const std::string& remotePath) {
    Message msg;
    msg.header.type = MSG_FILE_DELETE_REQUEST;
    
    writeString(msg.payload, remotePath);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createFileDeleteResponse(bool success) {
    Message msg;
    msg.header.type = MSG_FILE_DELETE_RESPONSE;
    
    writeBool(msg.payload, success);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

bool Protocol::parseFileDeleteRequest(const Message& msg, std::string& path) {
    if (msg.header.type != MSG_FILE_DELETE_REQUEST) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    return readString(ptr, end, path);
}

// ============================================================================
// 登录消息
// ============================================================================
Protocol::Message Protocol::createLoginRequest(const std::string& username, 
                                               const std::string& password) {
    Message msg;
    msg.header.type = MSG_LOGIN_REQUEST;
    
    writeString(msg.payload, username);
    writeString(msg.payload, password);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createLoginResponse(bool success, uint32_t sessionId, 
                                                uint32_t userId, const std::string& role) {
    Message msg;
    msg.header.type = MSG_LOGIN_RESPONSE;
    
    writeBool(msg.payload, success);
    
    if (success) {
        writeUint32(msg.payload, sessionId);
        writeUint32(msg.payload, userId);
        writeString(msg.payload, role);
    }
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

bool Protocol::parseLoginRequest(const Message& msg, std::string& username, 
                                 std::string& password) {
    if (msg.header.type != MSG_LOGIN_REQUEST) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    if (!readString(ptr, end, username)) return false;
    if (!readString(ptr, end, password)) return false;
    
    return true;
}

bool Protocol::parseLoginResponse(const Message& msg, bool& success, uint32_t& sessionId,
                                  uint32_t& userId, std::string& role) {
    if (msg.header.type != MSG_LOGIN_RESPONSE) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    if (!readBool(ptr, end, success)) return false;
    
    if (success) {
        if (!readUint32(ptr, end, sessionId)) return false;
        if (!readUint32(ptr, end, userId)) return false;
        if (!readString(ptr, end, role)) return false;
    }
    
    return true;
}

// ============================================================================
// 登出消息
// ============================================================================
Protocol::Message Protocol::createLogoutRequest(uint32_t sessionId) {
    Message msg;
    msg.header.type = MSG_LOGOUT_REQUEST;
    
    writeUint32(msg.payload, sessionId);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createLogoutResponse(bool success) {
    Message msg;
    msg.header.type = MSG_LOGOUT_RESPONSE;
    
    writeBool(msg.payload, success);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

// ============================================================================
// 注册消息
// ============================================================================
Protocol::Message Protocol::createRegisterRequest(const std::string& username, 
                                                  const std::string& password,
                                                  const std::string& role) {
    Message msg;
    msg.header.type = MSG_REGISTER_REQUEST;
    
    writeString(msg.payload, username);
    writeString(msg.payload, password);
    writeString(msg.payload, role);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createRegisterResponse(bool success, const std::string& message) {
    Message msg;
    msg.header.type = MSG_REGISTER_RESPONSE;
    
    writeBool(msg.payload, success);
    writeString(msg.payload, message);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

bool Protocol::parseRegisterRequest(const Message& msg, std::string& username,
                                    std::string& password, std::string& role) {
    if (msg.header.type != MSG_REGISTER_REQUEST) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    if (!readString(ptr, end, username)) return false;
    if (!readString(ptr, end, password)) return false;
    if (!readString(ptr, end, role)) return false;
    
    return true;
}

bool Protocol::parseRegisterResponse(const Message& msg, bool& success, std::string& message) {
    if (msg.header.type != MSG_REGISTER_RESPONSE) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    if (!readBool(ptr, end, success)) return false;
    if (!readString(ptr, end, message)) return false;
    
    return true;
}

// ============================================================================
// 论文提交消息
// ============================================================================
Protocol::Message Protocol::createSubmitPaperRequest(uint32_t sessionId, const std::string& title,
                                                     const std::string& abstract,
                                                     const std::vector<char>& fileData) {
    Message msg;
    msg.header.type = MSG_SUBMIT_PAPER_REQUEST;
    
    writeUint32(msg.payload, sessionId);
    writeString(msg.payload, title);
    writeString(msg.payload, abstract);
    writeUint32(msg.payload, fileData.size());
    
    size_t offset = msg.payload.size();
    msg.payload.resize(offset + fileData.size());
    std::memcpy(msg.payload.data() + offset, fileData.data(), fileData.size());
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createSubmitPaperResponse(bool success, uint32_t paperId, 
                                                      const std::string& message) {
    Message msg;
    msg.header.type = MSG_SUBMIT_PAPER_RESPONSE;
    
    writeBool(msg.payload, success);
    writeUint32(msg.payload, paperId);
    writeString(msg.payload, message);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

bool Protocol::parseSubmitPaperRequest(const Message& msg, uint32_t& sessionId,
                                       std::string& title, std::string& abstract,
                                       std::vector<char>& fileData) {
    if (msg.header.type != MSG_SUBMIT_PAPER_REQUEST) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    if (!readUint32(ptr, end, sessionId)) return false;
    if (!readString(ptr, end, title)) return false;
    if (!readString(ptr, end, abstract)) return false;
    
    uint32_t dataSize;
    if (!readUint32(ptr, end, dataSize)) return false;
    
    if (ptr + dataSize > end) return false;
    
    fileData.assign(ptr, ptr + dataSize);
    
    return true;
}

bool Protocol::parseSubmitPaperResponse(const Message& msg, bool& success, 
                                        uint32_t& paperId, std::string& message) {
    if (msg.header.type != MSG_SUBMIT_PAPER_RESPONSE) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    if (!readBool(ptr, end, success)) return false;
    if (!readUint32(ptr, end, paperId)) return false;
    if (!readString(ptr, end, message)) return false;
    
    return true;
}

// ============================================================================
// 上传修订版本
// ============================================================================
Protocol::Message Protocol::createUploadRevisionRequest(uint32_t sessionId, uint32_t paperId,
                                                        const std::vector<char>& fileData) {
    Message msg;
    msg.header.type = MSG_UPLOAD_REVISION_REQUEST;
    
    writeUint32(msg.payload, sessionId);
    writeUint32(msg.payload, paperId);
    writeUint32(msg.payload, fileData.size());
    
    size_t offset = msg.payload.size();
    msg.payload.resize(offset + fileData.size());
    std::memcpy(msg.payload.data() + offset, fileData.data(), fileData.size());
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createUploadRevisionResponse(bool success, const std::string& message) {
    Message msg;
    msg.header.type = MSG_UPLOAD_REVISION_RESPONSE;
    
    writeBool(msg.payload, success);
    writeString(msg.payload, message);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

// ============================================================================
// 获取我的论文
// ============================================================================
Protocol::Message Protocol::createGetMyPapersRequest(uint32_t sessionId) {
    Message msg;
    msg.header.type = MSG_GET_MY_PAPERS_REQUEST;
    
    writeUint32(msg.payload, sessionId);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createGetMyPapersResponse(bool success, const std::vector<PaperInfo>& papers) {
    Message msg;
    msg.header.type = MSG_GET_MY_PAPERS_RESPONSE;
    
    writeBool(msg.payload, success);
    writeUint32(msg.payload, papers.size());
    
    for (const auto& paper : papers) {
        writePaperInfo(msg.payload, paper);
    }
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

bool Protocol::parseGetMyPapersRequest(const Message& msg, uint32_t& sessionId) {
    if (msg.header.type != MSG_GET_MY_PAPERS_REQUEST) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    return readUint32(ptr, end, sessionId);
}

bool Protocol::parseGetMyPapersResponse(const Message& msg, bool& success, 
                                        std::vector<PaperInfo>& papers) {
    if (msg.header.type != MSG_GET_MY_PAPERS_RESPONSE) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    if (!readBool(ptr, end, success)) return false;
    
    uint32_t count;
    if (!readUint32(ptr, end, count)) return false;
    
    papers.clear();
    papers.reserve(count);
    
    for (uint32_t i = 0; i < count; i++) {
        PaperInfo paper;
        if (!readPaperInfo(ptr, end, paper)) return false;
        papers.push_back(paper);
    }
    
    return true;
}

// ============================================================================
// 获取待审论文
// ============================================================================
Protocol::Message Protocol::createGetPapersToReviewRequest(uint32_t sessionId) {
    Message msg;
    msg.header.type = MSG_GET_PAPERS_TO_REVIEW_REQUEST;
    
    writeUint32(msg.payload, sessionId);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createGetPapersToReviewResponse(bool success, 
                                                            const std::vector<PaperInfo>& papers) {
    Message msg;
    msg.header.type = MSG_GET_PAPERS_TO_REVIEW_RESPONSE;
    
    writeBool(msg.payload, success);
    writeUint32(msg.payload, papers.size());
    
    for (const auto& paper : papers) {
        writePaperInfo(msg.payload, paper);
    }
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

bool Protocol::parseGetPapersToReviewRequest(const Message& msg, uint32_t& sessionId) {
    if (msg.header.type != MSG_GET_PAPERS_TO_REVIEW_REQUEST) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    return readUint32(ptr, end, sessionId);
}

bool Protocol::parseGetPapersToReviewResponse(const Message& msg, bool& success,
                                              std::vector<PaperInfo>& papers) {
    if (msg.header.type != MSG_GET_PAPERS_TO_REVIEW_RESPONSE) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    if (!readBool(ptr, end, success)) return false;
    
    uint32_t count;
    if (!readUint32(ptr, end, count)) return false;
    
    papers.clear();
    papers.reserve(count);
    
    for (uint32_t i = 0; i < count; i++) {
        PaperInfo paper;
        if (!readPaperInfo(ptr, end, paper)) return false;
        papers.push_back(paper);
    }
    
    return true;
}

// ============================================================================
// 提交评审
// ============================================================================
Protocol::Message Protocol::createSubmitReviewRequest(uint32_t sessionId, uint32_t paperId,
                                                      const std::string& decision, int confidence,
                                                      const std::string& comments) {
    Message msg;
    msg.header.type = MSG_SUBMIT_REVIEW_REQUEST;
    
    writeUint32(msg.payload, sessionId);
    writeUint32(msg.payload, paperId);
    writeString(msg.payload, decision);
    writeInt32(msg.payload, confidence);
    writeString(msg.payload, comments);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createSubmitReviewResponse(bool success, uint32_t reviewId,
                                                       const std::string& message) {
    Message msg;
    msg.header.type = MSG_SUBMIT_REVIEW_RESPONSE;
    
    writeBool(msg.payload, success);
    writeUint32(msg.payload, reviewId);
    writeString(msg.payload, message);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

bool Protocol::parseSubmitReviewRequest(const Message& msg, uint32_t& sessionId, uint32_t& paperId,
                                        std::string& decision, int& confidence, std::string& comments) {
    if (msg.header.type != MSG_SUBMIT_REVIEW_REQUEST) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    if (!readUint32(ptr, end, sessionId)) return false;
    if (!readUint32(ptr, end, paperId)) return false;
    if (!readString(ptr, end, decision)) return false;
    
    int32_t conf;
    if (!readInt32(ptr, end, conf)) return false;
    confidence = conf;
    
    if (!readString(ptr, end, comments)) return false;
    
    return true;
}

bool Protocol::parseSubmitReviewResponse(const Message& msg, bool& success, 
                                         uint32_t& reviewId, std::string& message) {
    if (msg.header.type != MSG_SUBMIT_REVIEW_RESPONSE) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    if (!readBool(ptr, end, success)) return false;
    if (!readUint32(ptr, end, reviewId)) return false;
    if (!readString(ptr, end, message)) return false;
    
    return true;
}

// ============================================================================
// 分配审稿人
// ============================================================================
Protocol::Message Protocol::createAssignReviewerRequest(uint32_t sessionId, uint32_t paperId,
                                                        uint32_t reviewerId) {
    Message msg;
    msg.header.type = MSG_ASSIGN_REVIEWER_REQUEST;
    
    writeUint32(msg.payload, sessionId);
    writeUint32(msg.payload, paperId);
    writeUint32(msg.payload, reviewerId);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createAssignReviewerResponse(bool success, const std::string& message) {
    Message msg;
    msg.header.type = MSG_ASSIGN_REVIEWER_RESPONSE;
    
    writeBool(msg.payload, success);
    writeString(msg.payload, message);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

bool Protocol::parseAssignReviewerRequest(const Message& msg, uint32_t& sessionId,
                                          uint32_t& paperId, uint32_t& reviewerId) {
    if (msg.header.type != MSG_ASSIGN_REVIEWER_REQUEST) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    if (!readUint32(ptr, end, sessionId)) return false;
    if (!readUint32(ptr, end, paperId)) return false;
    if (!readUint32(ptr, end, reviewerId)) return false;
    
    return true;
}

// ============================================================================
// 获取所有论文 (编辑)
// ============================================================================
Protocol::Message Protocol::createGetAllPapersRequest(uint32_t sessionId) {
    Message msg;
    msg.header.type = MSG_GET_ALL_PAPERS_REQUEST;
    
    writeUint32(msg.payload, sessionId);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createGetAllPapersResponse(bool success, 
                                                       const std::vector<PaperInfo>& papers) {
    Message msg;
    msg.header.type = MSG_GET_ALL_PAPERS_RESPONSE;
    
    writeBool(msg.payload, success);
    writeUint32(msg.payload, papers.size());
    
    for (const auto& paper : papers) {
        writePaperInfo(msg.payload, paper);
    }
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

bool Protocol::parseGetAllPapersRequest(const Message& msg, uint32_t& sessionId) {
    if (msg.header.type != MSG_GET_ALL_PAPERS_REQUEST) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    return readUint32(ptr, end, sessionId);
}

bool Protocol::parseGetAllPapersResponse(const Message& msg, bool& success,
                                         std::vector<PaperInfo>& papers) {
    if (msg.header.type != MSG_GET_ALL_PAPERS_RESPONSE) return false;
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    if (!readBool(ptr, end, success)) return false;
    
    uint32_t count;
    if (!readUint32(ptr, end, count)) return false;
    
    papers.clear();
    papers.reserve(count);
    
    for (uint32_t i = 0; i < count; i++) {
        PaperInfo paper;
        if (!readPaperInfo(ptr, end, paper)) return false;
        papers.push_back(paper);
    }
    
    return true;
}

// ============================================================================
// 下载论文
// ============================================================================
Protocol::Message Protocol::createDownloadPaperRequest(uint32_t sessionId, uint32_t paperId) {
    Message msg;
    msg.header.type = MSG_DOWNLOAD_PAPER_REQUEST;
    
    writeUint32(msg.payload, sessionId);
    writeUint32(msg.payload, paperId);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createDownloadPaperResponse(bool success, const std::vector<char>& data) {
    Message msg;
    msg.header.type = MSG_DOWNLOAD_PAPER_RESPONSE;
    
    writeBool(msg.payload, success);
    
    if (success) {
        writeUint32(msg.payload, data.size());
        
        size_t offset = msg.payload.size();
        msg.payload.resize(offset + data.size());
        std::memcpy(msg.payload.data() + offset, data.data(), data.size());
    }
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

// ============================================================================
// 获取评审
// ============================================================================
Protocol::Message Protocol::createGetReviewsRequest(uint32_t sessionId, uint32_t paperId) {
    Message msg;
    msg.header.type = MSG_GET_REVIEWS_REQUEST;
    
    writeUint32(msg.payload, sessionId);
    writeUint32(msg.payload, paperId);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createGetReviewsResponse(bool success, const std::vector<ReviewInfo>& reviews) {
    Message msg;
    msg.header.type = MSG_GET_REVIEWS_RESPONSE;
    
    writeBool(msg.payload, success);
    writeUint32(msg.payload, reviews.size());
    
    for (const auto& review : reviews) {
        writeReviewInfo(msg.payload, review);
    }
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

// ============================================================================
// 编辑决定
// ============================================================================
Protocol::Message Protocol::createMakeDecisionRequest(uint32_t sessionId, uint32_t paperId,
                                                      const std::string& decision) {
    Message msg;
    msg.header.type = MSG_MAKE_DECISION_REQUEST;
    
    writeUint32(msg.payload, sessionId);
    writeUint32(msg.payload, paperId);
    writeString(msg.payload, decision);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createMakeDecisionResponse(bool success, const std::string& message) {
    Message msg;
    msg.header.type = MSG_MAKE_DECISION_RESPONSE;
    
    writeBool(msg.payload, success);
    writeString(msg.payload, message);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

// ============================================================================
// 统计信息
// ============================================================================
Protocol::Message Protocol::createGetStatisticsRequest(uint32_t sessionId) {
    Message msg;
    msg.header.type = MSG_GET_STATISTICS_REQUEST;
    
    writeUint32(msg.payload, sessionId);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

Protocol::Message Protocol::createGetStatisticsResponse(bool success, const std::string& stats) {
    Message msg;
    msg.header.type = MSG_GET_STATISTICS_RESPONSE;
    
    writeBool(msg.payload, success);
    writeString(msg.payload, stats);
    
    msg.header.length = msg.payload.size();
    msg.header.checksum = calculateChecksum(msg.payload);
    
    return msg;
}

bool Protocol::parseGetReviewsResponse(const Message& msg, bool& success, std::vector<ReviewInfo>& reviews) {
    if (msg.payload.empty()) {
        return false;
    }
    
    reviews.clear();
    
    const char* ptr = msg.payload.data();
    const char* end = ptr + msg.payload.size();
    
    // 解析 success 标志
    if (!readBool(ptr, end, success)) return false;
    
    if (!success) {
        return true;
    }
    
    // 解析评审数量
    uint32_t count;
    if (!readUint32(ptr, end, count)) return false;
    
    // 解析每个评审
    for (uint32_t i = 0; i < count; i++) {
        ReviewInfo review;
        if (!readReviewInfo(ptr, end, review)) return false;
        reviews.push_back(review);
    }
    
    return true;
}