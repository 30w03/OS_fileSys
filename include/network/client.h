#pragma once

#include "network/connection.h"
#include "protocol/protocol.h"
#include <string>
#include <memory>
#include <vector>

// 🔥 移除这里的 Statistics 定义，因为已经在 protocol.h 中定义了

class Client {
public:
    Client();
    ~Client();
    
    // 连接管理
    bool connect(const std::string& host, uint16_t port);
    void disconnect();
    bool isConnected() const;
    
    // 基础操作
    bool ping();
    
    // 文件操作
    std::vector<FileListEntry> listFiles();
    bool uploadFile(const std::string& localPath, const std::string& remotePath);
    bool downloadFile(const std::string& remotePath, const std::string& localPath);
    bool deleteFile(const std::string& remotePath);
    
    // 用户管理
    bool registerUser(const std::string& username, const std::string& password, const std::string& role);
    bool login(const std::string& username, const std::string& password);
    bool logout();
    
    // 论文管理
    bool submitPaper(const std::string& title, const std::string& abstract, const std::vector<char>& content);
    std::vector<PaperInfo> getMyPapers();
    std::vector<PaperInfo> getAllPapers();
    
    // 评审管理
    std::vector<PaperInfo> getPapersToReview();
    bool submitReview(uint32_t paperId, const std::string& decision, int score, const std::string& comment);
    
    // 编辑操作
    bool assignReviewer(uint32_t paperId, uint32_t reviewerId);
    
    // 统计信息
    Statistics getStatistics();

    // 显示我的论文（带评审详情）
    void viewMyPapers();
    
private:
    std::unique_ptr<Connection> connection_;
    bool connected_;
    uint32_t sessionId_;  // 会话ID
    
    // 🔥 辅助函数：获取指定论文的评审详情
    std::vector<ReviewInfo> getReviewsForPaper(uint32_t paperId);
};