#pragma once

#include "protocol/protocol.h"
#include <string>
#include <memory>
#include <cstdint>

class Connection {
public:
    explicit Connection(int socket);
    ~Connection();
    
    // 发送和接收消息
    bool sendMessage(const Protocol::Message& msg);
    bool receiveMessage(Protocol::Message& msg);
    
    // 连接信息
    std::string getRemoteAddress() const;
    uint16_t getRemotePort() const;
    
    // 关闭连接
    void close();
    bool isConnected() const;
    
private:
    int socket_;
    bool connected_;
    
    // 辅助函数
    bool sendAll(const char* data, size_t size);
    bool receiveAll(char* buffer, size_t size);
};
