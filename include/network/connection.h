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
    
    // 原始数据读取（用于HTTP请求检测）
    bool receiveRawData(char* buffer, size_t size);
    bool peekFirstBytes(char* buffer, size_t size);
    
    // HTTP响应发送
    bool sendHttpResponse(const std::string& response);
    
    // 获取socket描述符
    int getSocket() const { return socket_; }
    
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
