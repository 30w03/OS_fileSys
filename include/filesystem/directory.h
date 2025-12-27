#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include "common/config.h"

struct DirectoryEntry {
    uint32_t inodeNum;
    char name[Config::MAX_FILENAME];
    uint8_t nameLen;
    
    DirectoryEntry() {
        inodeNum = 0;
        nameLen = 0;
        std::memset(name, 0, Config::MAX_FILENAME);
    }
    
    DirectoryEntry(uint32_t inode, const std::string& fileName) {
        inodeNum = inode;
        nameLen = std::min(fileName.length(), (size_t)Config::MAX_FILENAME - 1);
        std::memset(name, 0, Config::MAX_FILENAME);
        std::memcpy(name, fileName.c_str(), nameLen);
    }
    
    std::string getName() const {
        return std::string(name, nameLen);
    }
    
    bool isValid() const {
        return inodeNum != 0;
    }
    
    void serialize(char* buffer) const {
        std::memcpy(buffer, this, sizeof(DirectoryEntry));
    }
    
    void deserialize(const char* buffer) {
        std::memcpy(this, buffer, sizeof(DirectoryEntry));
    }
};

#endif
