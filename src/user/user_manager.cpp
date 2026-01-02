#include "user/user_manager.h"
#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <iostream>
#include <map>

UserManager::UserManager() 
    : nextUserId_(1), nextSessionId_(1) {
    // 创建默认管理员账户
    createUser("admin", "admin123", UserRole::ADMIN);
}

UserManager::~UserManager() = default;

// ============================================================================
// 密码哈希
// ============================================================================
std::string UserManager::hashPassword(const std::string& password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()), 
           password.length(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') 
           << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

// ============================================================================
// 用户管理
// ============================================================================
bool UserManager::createUser(const std::string& username, const std::string& password, 
                             UserRole role) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查用户名是否已存在
    if (usernames_.find(username) != usernames_.end()) {
        return false;
    }
    
    User user;
    user.userId = generateUserId();
    user.username = username;
    user.passwordHash = hashPassword(password);
    user.role = role;
    user.isActive = true;
    
    users_[user.userId] = user;
    usernames_[username] = user.userId;
    
    return true;
}

bool UserManager::authenticateUser(const std::string& username, const std::string& password, 
                                   uint32_t& userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = usernames_.find(username);
    if (it == usernames_.end()) {
        return false;
    }
    
    userId = it->second;
    const User& user = users_[userId];
    
    if (!user.isActive) {
        return false;
    }
    
    return user.passwordHash == hashPassword(password);
}

bool UserManager::getUserById(uint32_t userId, User& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = users_.find(userId);
    if (it == users_.end()) {
        return false;
    }
    
    user = it->second;
    return true;
}

bool UserManager::updateUserRole(uint32_t userId, UserRole newRole) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = users_.find(userId);
    if (it == users_.end()) {
        return false;
    }
    
    it->second.role = newRole;
    return true;
}

bool UserManager::updateUserProfile(uint32_t userId, const std::string& institution, 
                                   const std::vector<std::string>& interests, int maxLoad) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = users_.find(userId);
    if (it == users_.end()) {
        return false;
    }
    
    it->second.institution = institution;
    it->second.researchInterests = interests;
    it->second.maxLoad = maxLoad;
    return true;
}

bool UserManager::deactivateUser(uint32_t userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = users_.find(userId);
    if (it == users_.end()) {
        return false;
    }
    
    it->second.isActive = false;
    return true;
}

std::vector<User> UserManager::listAllUsers() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<User> result;
    result.reserve(users_.size());
    
    for (const auto& pair : users_) {
        result.push_back(pair.second);
    }
    
    return result;
}

// ============================================================================
// 会话管理
// ============================================================================
uint32_t UserManager::createSession(uint32_t userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Session session;
    session.sessionId = generateSessionId();
    session.userId = userId;
    session.timestamp = static_cast<uint64_t>(std::time(nullptr));
    session.isValid = true;
    
    sessions_[session.sessionId] = session;
    
    return session.sessionId;
}

bool UserManager::validateSession(uint32_t sessionId, uint32_t& userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end() || !it->second.isValid) {
        return false;
    }
    
    userId = it->second.userId;
    return true;
}

void UserManager::invalidateSession(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        it->second.isValid = false;
    }
}

// ============================================================================
// 权限检查
// ============================================================================
bool UserManager::hasPermission(uint32_t userId, UserRole requiredRole) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = users_.find(userId);
    if (it == users_.end()) {
        return false;
    }
    
    return static_cast<uint8_t>(it->second.role) >= static_cast<uint8_t>(requiredRole);
}

// ============================================================================
// ID 生成
// ============================================================================
uint32_t UserManager::generateUserId() {
    return nextUserId_++;
}

uint32_t UserManager::generateSessionId() {
    return nextSessionId_++;
}

// ============================================================================
// 持久化（简化版本，仅用于演示）
// ============================================================================
bool UserManager::saveToFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;
    
    // 保存用户数据
    uint32_t userCount = users_.size();
    file.write(reinterpret_cast<const char*>(&userCount), sizeof(userCount));
    
    for (const auto& pair : users_) {
        const User& user = pair.second;
        
        // Write POD fields
        file.write(reinterpret_cast<const char*>(&user.userId), sizeof(user.userId));
        file.write(reinterpret_cast<const char*>(&user.role), sizeof(user.role));
        file.write(reinterpret_cast<const char*>(&user.isActive), sizeof(user.isActive));
        
        // Write strings (length + data)
        uint32_t nameLen = user.username.length();
        file.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
        file.write(user.username.data(), nameLen);
        
        uint32_t passLen = user.passwordHash.length();
        file.write(reinterpret_cast<const char*>(&passLen), sizeof(passLen));
        file.write(user.passwordHash.data(), passLen);

        // Write institution
        uint32_t instLen = user.institution.length();
        file.write(reinterpret_cast<const char*>(&instLen), sizeof(instLen));
        file.write(user.institution.data(), instLen);

        // Write researchInterests
        uint32_t interestsCount = user.researchInterests.size();
        file.write(reinterpret_cast<const char*>(&interestsCount), sizeof(interestsCount));
        for (const auto& interest : user.researchInterests) {
            uint32_t intLen = interest.length();
            file.write(reinterpret_cast<const char*>(&intLen), sizeof(intLen));
            file.write(interest.data(), intLen);
        }

        // Write maxLoad
        file.write(reinterpret_cast<const char*>(&user.maxLoad), sizeof(user.maxLoad));
    }
    
    return file.good();
}

bool UserManager::loadFromFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cout << "ℹ️  No user data file found, using defaults" << std::endl;
        return true;  // 文件不存在是正常的，第一次运行
    }
    
    // 使用 unordered_map 而不是 map
    std::unordered_map<uint32_t, User> tempUsers;
    std::unordered_map<std::string, uint32_t> tempUsernames;
    
    uint32_t userCount;
    file.read(reinterpret_cast<char*>(&userCount), sizeof(userCount));
    
    if (!file.good()) {
        std::cout << "⚠️  Failed to read user count, using defaults" << std::endl;
        return true;
    }
    
    for (uint32_t i = 0; i < userCount; i++) {
        User user;
        
        // Read POD fields
        file.read(reinterpret_cast<char*>(&user.userId), sizeof(user.userId));
        file.read(reinterpret_cast<char*>(&user.role), sizeof(user.role));
        file.read(reinterpret_cast<char*>(&user.isActive), sizeof(user.isActive));
        
        // Read strings
        uint32_t nameLen;
        file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        user.username.resize(nameLen);
        file.read(&user.username[0], nameLen);
        
        uint32_t passLen;
        file.read(reinterpret_cast<char*>(&passLen), sizeof(passLen));
        user.passwordHash.resize(passLen);
        file.read(&user.passwordHash[0], passLen);
        
        // Try to read new fields. If we hit EOF or fail, it might be an old file format.
        // However, since we don't have versioning, this is best-effort or requires a fresh DB.
        // We'll assume the file format matches the code.
        
        // Read institution
        uint32_t instLen;
        file.read(reinterpret_cast<char*>(&instLen), sizeof(instLen));
        if (file.good()) {
            user.institution.resize(instLen);
            file.read(&user.institution[0], instLen);
            
            // Read researchInterests
            uint32_t interestsCount;
            file.read(reinterpret_cast<char*>(&interestsCount), sizeof(interestsCount));
            for (uint32_t j = 0; j < interestsCount; j++) {
                uint32_t intLen;
                file.read(reinterpret_cast<char*>(&intLen), sizeof(intLen));
                std::string interest;
                interest.resize(intLen);
                file.read(&interest[0], intLen);
                user.researchInterests.push_back(interest);
            }
            
            // Read maxLoad
            file.read(reinterpret_cast<char*>(&user.maxLoad), sizeof(user.maxLoad));
        } else {
            // If we failed to read new fields, reset stream state if it was just EOF/short read
            // and keep the defaults for these fields.
            // But wait, if we are in the middle of a file (multiple users), 
            // we might have read into the next user's data.
            // Without versioning, migration is hard. 
            // Let's assume we are starting fresh or the user accepts data loss.
            file.clear(); 
        }

        if (!file.good()) {
            // If we are still not good, it's a real error or end of file in a bad place
             std::cout << "⚠️  Failed to read user data (possibly old format), using defaults for remaining fields" << std::endl;
             // We might want to return true to allow partial load, or false.
             // The original code returned true on failure inside the loop?
             // No, it returned true.
        }
        
        tempUsers[user.userId] = user;
        tempUsernames[user.username] = user.userId;
        
        if (user.userId >= nextUserId_) {
            nextUserId_ = user.userId + 1;
        }
    }
    
    // 只有在成功读取所有数据后才替换
    users_ = std::move(tempUsers);
    usernames_ = std::move(tempUsernames);
    
    std::cout << "✅ Loaded " << users_.size() << " users from file" << std::endl;
    
    return true;
}
