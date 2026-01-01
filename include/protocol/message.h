#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <ctime>
#include <arpa/inet.h> // For htonl, ntohl

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
struct MessageHeader {
    uint32_t magic;        // 魔数：0x50525346 ("PRSF")
    uint8_t version;       // 协议版本
    uint8_t type;          // 消息类型 (Changed to uint8_t for alignment)
    uint16_t reserved;     // 保留字段 (For alignment)
    uint32_t length;       // 消息体长度
    uint64_t timestamp;    // 时间戳
    uint32_t checksum;     // CRC32 校验和
    
    MessageHeader() 
        : magic(0x50525346), version(1), type(0), reserved(0),
          length(0), timestamp(0), checksum(0) {}
    
    bool isValid() const {
        return magic == 0x50525346 && version == 1;
    }
    
    // 序列化：主机字节序 -> 网络字节序
    void serialize(char* buffer) const {
        uint32_t n_magic = htonl(magic);
        uint32_t n_length = htonl(length);
        uint32_t n_checksum = htonl(checksum);
        
        // 64位整数处理
        uint32_t high = htonl(static_cast<uint32_t>(timestamp >> 32));
        uint32_t low = htonl(static_cast<uint32_t>(timestamp & 0xFFFFFFFF));
        
        size_t offset = 0;
        std::memcpy(buffer + offset, &n_magic, 4); offset += 4;
        std::memcpy(buffer + offset, &version, 1); offset += 1;
        std::memcpy(buffer + offset, &type, 1);    offset += 1;
        std::memcpy(buffer + offset, &reserved, 2); offset += 2; // Padding
        std::memcpy(buffer + offset, &n_length, 4); offset += 4;
        
        // Timestamp (Big Endian)
        std::memcpy(buffer + offset, &high, 4); offset += 4;
        std::memcpy(buffer + offset, &low, 4);  offset += 4;
        
        std::memcpy(buffer + offset, &n_checksum, 4);
    }
    
    // 反序列化：网络字节序 -> 主机字节序
    void deserialize(const char* buffer) {
        size_t offset = 0;
        
        uint32_t n_magic;
        std::memcpy(&n_magic, buffer + offset, 4); offset += 4;
        magic = ntohl(n_magic);
        
        std::memcpy(&version, buffer + offset, 1); offset += 1;
        std::memcpy(&type, buffer + offset, 1);    offset += 1;
        std::memcpy(&reserved, buffer + offset, 2); offset += 2;
        
        uint32_t n_length;
        std::memcpy(&n_length, buffer + offset, 4); offset += 4;
        length = ntohl(n_length);
        
        uint32_t high, low;
        std::memcpy(&high, buffer + offset, 4); offset += 4;
        std::memcpy(&low, buffer + offset, 4);  offset += 4;
        timestamp = (static_cast<uint64_t>(ntohl(high)) << 32) | ntohl(low);
        
        uint32_t n_checksum;
        std::memcpy(&n_checksum, buffer + offset, 4);
        checksum = ntohl(n_checksum);
    }
    
    static constexpr size_t SIZE = 24;  // 固定大小
};

// 消息结构
struct Message {
    MessageHeader header;
    std::vector<char> payload;
    
    Message() = default;
    
    Message(MessageType type) {
        header.type = static_cast<uint8_t>(type);
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
