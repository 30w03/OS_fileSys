#pragma once
#include "user/user.h"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <string>

class UserManager {
public:
    UserManager();
    ~UserManager();
    
    // 用户管理
    bool createUser(const std::string& username, const std::string& password, UserRole role);
    bool authenticateUser(const std::string& username, const std::string& password, uint32_t& userId);
    bool getUserById(uint32_t userId, User& user);
    bool updateUserRole(uint32_t userId, UserRole newRole);
    bool deactivateUser(uint32_t userId);
    std::vector<User> listAllUsers();
    
    // 会话管理
    uint32_t createSession(uint32_t userId);
    bool validateSession(uint32_t sessionId, uint32_t& userId);
    void invalidateSession(uint32_t sessionId);
    
    // 权限检查
    bool hasPermission(uint32_t userId, UserRole requiredRole);
    
    // 持久化
    bool saveToFile(const std::string& filename);
    bool loadFromFile(const std::string& filename);
    // 统计信息
    uint32_t getUserCount() const { return users_.size(); }
    
private:
    std::string hashPassword(const std::string& password);
    uint32_t generateUserId();
    uint32_t generateSessionId();
    
    std::unordered_map<uint32_t, User> users_;           // userId -> User
    std::unordered_map<std::string, uint32_t> usernames_; // username -> userId
    std::unordered_map<uint32_t, Session> sessions_;     // sessionId -> Session
    
    uint32_t nextUserId_;
    uint32_t nextSessionId_;
    
    std::mutex mutex_;
};
