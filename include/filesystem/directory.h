#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include "common/config.h"
#include "filesystem/inode.h"

// 表示目录中的一个条目
struct DirectoryEntry {
    uint32_t inodeNum; // 关联的 Inode 编号
    char name[Config::MAX_FILENAME]; // 文件或目录的名称
    uint8_t nameLen; // 名称的实际长度

    // 默认构造函数：初始化为空条目
    DirectoryEntry() {
        inodeNum = INVALID_INODE; // 无效的 Inode 编号
        nameLen = 0; // 名称长度为 0
        std::memset(name, 0, Config::MAX_FILENAME); // 清空名称缓冲区
    }

    // 带参数的构造函数：根据给定的 Inode 编号和文件名初始化条目
    DirectoryEntry(uint32_t inode, const std::string& fileName) {
        inodeNum = inode; // 设置 Inode 编号
        nameLen = std::min(fileName.length(), (size_t)Config::MAX_FILENAME - 1); // 限制名称长度
        std::memset(name, 0, Config::MAX_FILENAME); // 清空名称缓冲区
        std::memcpy(name, fileName.c_str(), nameLen); // 复制文件名到缓冲区
    }

    // 获取条目的名称
    std::string getName() const {
        return std::string(name, nameLen); // 根据名称缓冲区和长度构造字符串
    }

    // 检查条目是否有效
    bool isValid() const {
        return inodeNum != INVALID_INODE && nameLen > 0; // Inode 编号有效且名称长度大于 0
    }

    // 将条目序列化到缓冲区
    void serialize(char* buffer) const {
        std::memcpy(buffer, this, sizeof(DirectoryEntry)); // 将结构体内容复制到缓冲区
    }

    // 从缓冲区反序列化条目
    void deserialize(const char* buffer) {
        std::memcpy(this, buffer, sizeof(DirectoryEntry)); // 从缓冲区复制数据到结构体
    }
};

#endif
