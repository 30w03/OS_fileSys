#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <ctime>

// 消息类型枚举
enum class MessageType : uint8_t {
    PING = 0,
    PONG = 1,
    
    FILE_LIST_REQUEST = 10,
    FILE_LIST_RESPONSE = 11,
    
    FILE_UPLOAD_REQUEST = 20,
    FILE_UPLOAD_ACK = 21,
    FILE_UPLOAD_DATA = 22,
    
    FILE_DOWNLOAD_REQUEST = 30,
    FILE_DOWNLOAD_ACK = 31,
    FILE_DOWNLOAD_DATA = 32,
    
    FILE_DELETE_REQUEST = 40,
    FILE_DELETE_ACK = 41,
    
    ERROR = 255
};

// 消息头结构
#pragma pack(push, 1)
struct MessageHeader {
    uint32_t magic;        // 魔数：0x50525346 ("PRSF")
    uint8_t version;       // 协议版本
    MessageType type;      // 消息类型
    uint32_t length;       // 消息体长度
    uint64_t timestamp;    // 时间戳
    uint32_t checksum;     // 校验和
    
    MessageHeader() 
        : magic(0x50525346), version(1), type(MessageType::PING),
          length(0), timestamp(0), checksum(0) {}
    
    bool isValid() const {
        return magic == 0x50525346 && version == 1;
    }
    
    void serialize(char* buffer) const {
        std::memcpy(buffer, this, sizeof(MessageHeader));
    }
    
    void deserialize(const char* buffer) {
        std::memcpy(this, buffer, sizeof(MessageHeader));
    }
    
    static constexpr size_t SIZE = 24;  // 固定大小
};
#pragma pack(pop)

// 消息结构
struct Message {
    MessageHeader header;
    std::vector<char> payload;
    
    Message() = default;
    
    Message(MessageType type) {
        header.type = type;
        header.timestamp = static_cast<uint64_t>(std::time(nullptr));
    }
    
    void setPayload(const std::vector<char>& data) {
        payload = data;
        header.length = static_cast<uint32_t>(payload.size());
    }
    
    void setPayload(const std::string& str) {
        payload.assign(str.begin(), str.end());
        header.length = static_cast<uint32_t>(payload.size());
    }
    
    std::string getPayloadAsString() const {
        return std::string(payload.begin(), payload.end());
    }
};

// 文件列表条目
#pragma pack(push, 1)
struct FileListEntry {
    char path[256];
    uint32_t size;
    uint8_t isDirectory;
    
    FileListEntry() : size(0), isDirectory(0) {
        std::memset(path, 0, sizeof(path));
    }
    
    void setPath(const std::string& p) {
        std::strncpy(path, p.c_str(), sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }
    
    std::string getPath() const {
        return std::string(path);
    }
};
#pragma pack(pop)

// 文件上传请求
#pragma pack(push, 1)
struct FileUploadRequest {
    char path[256];
    uint32_t fileSize;
    
    FileUploadRequest() : fileSize(0) {
        std::memset(path, 0, sizeof(path));
    }
    
    void setPath(const std::string& p) {
        std::strncpy(path, p.c_str(), sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }
    
    std::string getPath() const {
        return std::string(path);
    }
};
#pragma pack(pop)

// 文件下载请求
#pragma pack(push, 1)
struct FileDownloadRequest {
    char path[256];
    
    FileDownloadRequest() {
        std::memset(path, 0, sizeof(path));
    }
    
    void setPath(const std::string& p) {
        std::strncpy(path, p.c_str(), sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }
    
    std::string getPath() const {
        return std::string(path);
    }
};
#pragma pack(pop)

// 文件数据块
#pragma pack(push, 1)
struct FileDataChunk {
    uint32_t offset;
    uint32_t size;
    char data[4096];
    
    FileDataChunk() : offset(0), size(0) {
        std::memset(data, 0, sizeof(data));
    }
};
#pragma pack(pop)

// 确认消息
#pragma pack(push, 1)
struct AckMessage {
    uint8_t success;
    char error[256];
    
    AckMessage() : success(0) {
        std::memset(error, 0, sizeof(error));
    }
    
    void setError(const std::string& err) {
        std::strncpy(error, err.c_str(), sizeof(error) - 1);
        error[sizeof(error) - 1] = '\0';
    }
    
    std::string getError() const {
        return std::string(error);
    }
};
#pragma pack(pop)

#endif
