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
    
    // Serialize header
    char headerBuf[Protocol::MessageHeader::SIZE];
    // Manual serialization to ensure Network Byte Order
    uint32_t n_magic = htonl(msg.header.magic);
    uint32_t n_length = htonl(msg.header.length);
    uint32_t n_checksum = htonl(msg.header.checksum);
    uint32_t high = htonl(static_cast<uint32_t>(msg.header.timestamp >> 32));
    uint32_t low = htonl(static_cast<uint32_t>(msg.header.timestamp & 0xFFFFFFFF));
    
    size_t offset = 0;
    std::memcpy(headerBuf + offset, &n_magic, 4); offset += 4;
    std::memcpy(headerBuf + offset, &msg.header.version, 1); offset += 1;
    std::memcpy(headerBuf + offset, &msg.header.type, 1);    offset += 1;
    std::memcpy(headerBuf + offset, &msg.header.reserved, 2); offset += 2;
    std::memcpy(headerBuf + offset, &n_length, 4); offset += 4;
    std::memcpy(headerBuf + offset, &high, 4); offset += 4;
    std::memcpy(headerBuf + offset, &low, 4);  offset += 4;
    std::memcpy(headerBuf + offset, &n_checksum, 4);
    
    // 1. Send header
    if (!sendAll(headerBuf, Protocol::MessageHeader::SIZE)) {
        return false;
    }
    
    // 2. Send payload
    if (msg.header.length > 0) {
        if (!sendAll(msg.payload.data(), msg.header.length)) {
            return false;
        }
    }
    
    return true;
}

bool Connection::receiveMessage(Protocol::Message& msg) {
    if (!connected_) {
        return false;
    }
    
    // 1. Receive header
    char headerBuf[Protocol::MessageHeader::SIZE];
    if (!receiveAll(headerBuf, Protocol::MessageHeader::SIZE)) {
        return false;
    }
    
    // Deserialize header
    size_t offset = 0;
    uint32_t n_magic;
    std::memcpy(&n_magic, headerBuf + offset, 4); offset += 4;
    msg.header.magic = ntohl(n_magic);
    
    std::memcpy(&msg.header.version, headerBuf + offset, 1); offset += 1;
    std::memcpy(&msg.header.type, headerBuf + offset, 1);    offset += 1;
    std::memcpy(&msg.header.reserved, headerBuf + offset, 2); offset += 2;
    
    uint32_t n_length;
    std::memcpy(&n_length, headerBuf + offset, 4); offset += 4;
    msg.header.length = ntohl(n_length);
    
    uint32_t high, low;
    std::memcpy(&high, headerBuf + offset, 4); offset += 4;
    std::memcpy(&low, headerBuf + offset, 4);  offset += 4;
    msg.header.timestamp = (static_cast<uint64_t>(ntohl(high)) << 32) | ntohl(low);
    
    uint32_t n_checksum;
    std::memcpy(&n_checksum, headerBuf + offset, 4);
    msg.header.checksum = ntohl(n_checksum);
    
    // 2. Verify magic
    if (msg.header.magic != 0x50525346) { // "PRSF"
        std::cerr << "Invalid magic number: " << std::hex << msg.header.magic << std::endl;
        return false;
    }
    
    // 3. Receive payload
    if (msg.header.length > 0) {
        msg.payload.resize(msg.header.length);
        if (!receiveAll(msg.payload.data(), msg.header.length)) {
            return false;
        }
        
        // 4. Verify checksum
        uint32_t checksum = Protocol::calculateChecksum(msg.payload);
        if (checksum != msg.header.checksum) {
            std::cerr << "Checksum mismatch" << std::endl;
            return false;
        }
    } else {
        msg.payload.clear();
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

bool Connection::receiveRawData(char* buffer, size_t size) {
    return receiveAll(buffer, size);
}

bool Connection::peekFirstBytes(char* buffer, size_t size) {
    if (!connected_) {
        return false;
    }
    
    ssize_t received = recv(socket_, buffer, size, MSG_PEEK);
    
    if (received <= 0) {
        connected_ = false;
        return false;
    }
    
    return true;
}

bool Connection::sendHttpResponse(const std::string& response) {
    if (!connected_) {
        return false;
    }
    
    return sendAll(response.c_str(), response.size());
}
