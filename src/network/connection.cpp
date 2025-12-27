#include "network/connection.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

Connection::Connection(int socket) 
    : socket_(socket), connected_(true) {}

Connection::~Connection() {
    close();
}

bool Connection::sendMessage(const Protocol::Message& msg) {
    if (!connected_) {
        return false;
    }
    
    // 1. 发送消息头
    if (!sendAll(reinterpret_cast<const char*>(&msg.header), sizeof(Protocol::MessageHeader))) {
        return false;
    }
    
    // 2. 发送消息体
    if (msg.header.payloadSize > 0) {
        if (!sendAll(msg.payload.data(), msg.header.payloadSize)) {
            return false;
        }
    }
    
    return true;
}

bool Connection::receiveMessage(Protocol::Message& msg) {
    if (!connected_) {
        return false;
    }
    
    // 1. 接收消息头
    if (!receiveAll(reinterpret_cast<char*>(&msg.header), sizeof(Protocol::MessageHeader))) {
        return false;
    }
    
    // 2. 验证魔数
    if (msg.header.magic != 0x12345678) {
        std::cerr << "Invalid magic number: " << std::hex << msg.header.magic << std::endl;
        return false;
    }
    
    // 3. 接收消息体
    if (msg.header.payloadSize > 0) {
        msg.payload.resize(msg.header.payloadSize);
        if (!receiveAll(msg.payload.data(), msg.header.payloadSize)) {
            return false;
        }
        
        // 4. 验证校验和
        uint32_t checksum = Protocol::calculateChecksum(msg.payload);
        if (checksum != msg.header.checksum) {
            std::cerr << "Checksum mismatch" << std::endl;
            return false;
        }
    }
    
    return true;
}

std::string Connection::getRemoteAddress() const {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    
    if (getpeername(socket_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        return inet_ntoa(addr.sin_addr);
    }
    
    return "unknown";
}

uint16_t Connection::getRemotePort() const {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    
    if (getpeername(socket_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        return ntohs(addr.sin_port);
    }
    
    return 0;
}

void Connection::close() {
    if (connected_) {
        ::close(socket_);
        connected_ = false;
    }
}

bool Connection::isConnected() const {
    return connected_;
}

bool Connection::sendAll(const char* data, size_t size) {
    size_t totalSent = 0;
    
    while (totalSent < size) {
        ssize_t sent = send(socket_, data + totalSent, size - totalSent, 0);
        
        if (sent <= 0) {
            connected_ = false;
            return false;
        }
        
        totalSent += sent;
    }
    
    return true;
}

bool Connection::receiveAll(char* buffer, size_t size) {
    size_t totalReceived = 0;
    
    while (totalReceived < size) {
        ssize_t received = recv(socket_, buffer + totalReceived, size - totalReceived, 0);
        
        if (received <= 0) {
            connected_ = false;
            return false;
        }
        
        totalReceived += received;
    }
    
    return true;
}
