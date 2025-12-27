#include "protocol/protocol.h"
#include "network/connection.h"
#include <cassert>
#include <iostream>
#include <cstring>

void testPingPong() {
    std::cout << "Testing Ping/Pong..." << std::endl;
    
    auto pingMsg = Protocol::createPingMessage();
    assert(pingMsg.header.type == Protocol::MSG_PING);
    assert(pingMsg.header.magic == 0x12345678);
    
    auto pongMsg = Protocol::createPongMessage();
    assert(pongMsg.header.type == Protocol::MSG_PONG);
    
    std::cout << "✓ Ping/Pong test passed" << std::endl;
}

void testFileListEntry() {
    std::cout << "Testing FileListEntry..." << std::endl;
    
    std::vector<FileListEntry> entries;
    
    FileListEntry entry1;
    entry1.filename = "test.txt";
    entry1.size = 1024;
    entry1.timestamp = 123456;
    entries.push_back(entry1);
    
    FileListEntry entry2;
    entry2.filename = "document.pdf";
    entry2.size = 2048;
    entry2.timestamp = 789012;
    entries.push_back(entry2);
    
    auto msg = Protocol::createFileListResponse(entries);
    assert(msg.header.type == Protocol::MSG_FILE_LIST_RESPONSE);
    
    std::vector<FileListEntry> parsed;
    assert(Protocol::parseFileListResponse(msg, parsed));
    assert(parsed.size() == 2);
    assert(parsed[0].filename == "test.txt");
    assert(parsed[0].size == 1024);
    assert(parsed[1].filename == "document.pdf");
    
    std::cout << "✓ FileListEntry test passed" << std::endl;
}

void testFileUpload() {
    std::cout << "Testing File Upload..." << std::endl;
    
    std::vector<char> data = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    auto msg = Protocol::createFileUploadRequest("/test.txt", data);
    
    assert(msg.header.type == Protocol::MSG_FILE_UPLOAD_REQUEST);
    
    std::string path;
    std::vector<char> parsedData;
    assert(Protocol::parseFileUploadRequest(msg, path, parsedData));
    assert(path == "/test.txt");
    assert(parsedData.size() == data.size());
    assert(std::memcmp(parsedData.data(), data.data(), data.size()) == 0);
    
    auto response = Protocol::createFileUploadResponse(true);
    assert(response.header.type == Protocol::MSG_FILE_UPLOAD_RESPONSE);
    assert(response.payload[0] == 1);
    
    std::cout << "✓ File Upload test passed" << std::endl;
}

void testFileDownload() {
    std::cout << "Testing File Download..." << std::endl;
    
    auto request = Protocol::createFileDownloadRequest("/download.txt");
    assert(request.header.type == Protocol::MSG_FILE_DOWNLOAD_REQUEST);
    
    std::string path;
    assert(Protocol::parseFileDownloadRequest(request, path));
    assert(path == "/download.txt");
    
    std::vector<char> data = {'T', 'e', 's', 't', ' ', 'D', 'a', 't', 'a'};
    auto response = Protocol::createFileDownloadResponse(true, data);
    assert(response.header.type == Protocol::MSG_FILE_DOWNLOAD_RESPONSE);
    
    bool success;
    std::vector<char> parsedData;
    assert(Protocol::parseFileDownloadResponse(response, success, parsedData));
    assert(success);
    assert(parsedData.size() == data.size());
    
    std::cout << "✓ File Download test passed" << std::endl;
}

void testFileDelete() {
    std::cout << "Testing File Delete..." << std::endl;
    
    auto request = Protocol::createFileDeleteRequest("/delete.txt");
    assert(request.header.type == Protocol::MSG_FILE_DELETE_REQUEST);
    
    std::string path;
    assert(Protocol::parseFileDeleteRequest(request, path));
    assert(path == "/delete.txt");
    
    auto response = Protocol::createFileDeleteResponse(true);
    assert(response.header.type == Protocol::MSG_FILE_DELETE_RESPONSE);
    assert(response.payload[0] == 1);
    
    std::cout << "✓ File Delete test passed" << std::endl;
}

void testAuthentication() {
    std::cout << "Testing Authentication..." << std::endl;
    
    // Test login request
    auto loginReq = Protocol::createLoginRequest("admin", "password123");
    assert(loginReq.header.type == Protocol::MSG_LOGIN_REQUEST);
    
    std::string username, password;
    assert(Protocol::parseLoginRequest(loginReq, username, password));
    assert(username == "admin");
    assert(password == "password123");
    
    // Test login response
    auto loginResp = Protocol::createLoginResponse(true, 42, 1, "Admin");
    assert(loginResp.header.type == Protocol::MSG_LOGIN_RESPONSE);
    
    bool success;
    uint32_t sessionId, userId;
    std::string role;
    assert(Protocol::parseLoginResponse(loginResp, success, sessionId, userId, role));
    assert(success);
    assert(sessionId == 42);
    assert(userId == 1);
    assert(role == "Admin");
    
    std::cout << "✓ Authentication test passed" << std::endl;
}

int main() {
    std::cout << "=== Network Protocol Tests ===" << std::endl;
    
    testPingPong();
    testFileListEntry();
    testFileUpload();
    testFileDownload();
    testFileDelete();
    testAuthentication();
    
    std::cout << "\n=== All tests passed! ===" << std::endl;
    return 0;
}
