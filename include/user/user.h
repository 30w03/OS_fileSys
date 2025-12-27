#pragma once
#include <string>
#include <cstdint>
#include <vector>

enum class UserRole : uint8_t {
    AUTHOR = 0,
    REVIEWER = 1,
    EDITOR = 2,
    ADMIN = 3
};

struct User {
    uint32_t userId;
    std::string username;
    std::string passwordHash;  // SHA-256 hash
    UserRole role;
    bool isActive;
    
    User() : userId(0), role(UserRole::AUTHOR), isActive(true) {}
    
    std::string getRoleName() const {
        switch (role) {
            case UserRole::AUTHOR: return "Author";
            case UserRole::REVIEWER: return "Reviewer";
            case UserRole::EDITOR: return "Editor";
            case UserRole::ADMIN: return "Admin";
            default: return "Unknown";
        }
    }
};

struct Session {
    uint32_t sessionId;
    uint32_t userId;
    uint64_t timestamp;  // 创建时间
    bool isValid;
    
    Session() : sessionId(0), userId(0), timestamp(0), isValid(false) {}
};
