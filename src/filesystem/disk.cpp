#include "filesystem/disk.h"
#include <iostream>
#include <cstring>

Disk::Disk(const std::string& filename)
    : filename_(filename), totalBlocks_(0), blockSize_(0) {
}

Disk::~Disk() {
    close();
}

bool Disk::create(uint32_t totalBlocks, uint32_t blockSize) {
    totalBlocks_ = totalBlocks;
    blockSize_ = blockSize;
    
    // 创建新文件
    file_.open(filename_, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file_.is_open()) {
        std::cerr << "Failed to create disk file: " << filename_ << std::endl;
        return false;
    }
    
    // 写入头信息
    file_.write(reinterpret_cast<const char*>(&totalBlocks_), sizeof(totalBlocks_));
    file_.write(reinterpret_cast<const char*>(&blockSize_), sizeof(blockSize_));
    
    // 分配空间
    char* zeroBlock = new char[blockSize_];
    memset(zeroBlock, 0, blockSize_);
    
    for (uint32_t i = 0; i < totalBlocks_; i++) {
        file_.write(zeroBlock, blockSize_);
    }
    
    delete[] zeroBlock;
    file_.close();
    
    return true;
}

bool Disk::open() {
    // 关闭已打开的文件
    if (file_.is_open()) {
        file_.close();
    }
    
    // 以读写模式打开
    file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
        std::cerr << "Failed to open disk file: " << filename_ << std::endl;
        return false;
    }
    
    // 读取头信息
    file_.read(reinterpret_cast<char*>(&totalBlocks_), sizeof(totalBlocks_));
    file_.read(reinterpret_cast<char*>(&blockSize_), sizeof(blockSize_));
    
    if (!file_.good()) {
        std::cerr << "Failed to read disk header" << std::endl;
        file_.close();
        return false;
    }
    
    return true;
}

void Disk::close() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool Disk::readBlock(uint32_t blockId, char* buffer) {
    if (!file_.is_open()) {
        // 静默失败，不输出警告
        return false;
    }
    
    if (blockId >= totalBlocks_) {
        std::cerr << "Block ID out of range: " << blockId << std::endl;
        return false;
    }
    
    std::streampos pos = 8 + static_cast<std::streampos>(blockId) * blockSize_;
    file_.seekg(pos);
    
    if (!file_.good()) {
        std::cerr << "Failed to seek to block " << blockId << std::endl;
        return false;
    }
    
    file_.read(buffer, blockSize_);
    
    if (!file_.good()) {
        std::cerr << "Failed to read block " << blockId << std::endl;
        return false;
    }
    
    return true;
}

bool Disk::writeBlock(uint32_t blockId, const char* buffer) {
    if (!file_.is_open()) {
        // 静默失败，不输出警告
        return false;
    }
    
    if (blockId >= totalBlocks_) {
        std::cerr << "Block ID out of range: " << blockId << std::endl;
        return false;
    }
    
    std::streampos pos = 8 + static_cast<std::streampos>(blockId) * blockSize_;
    file_.seekp(pos);
    
    if (!file_.good()) {
        std::cerr << "Failed to seek to block " << blockId << std::endl;
        return false;
    }
    
    file_.write(buffer, blockSize_);
    file_.flush();
    
    if (!file_.good()) {
        std::cerr << "Failed to write block " << blockId << std::endl;
        return false;
    }
    
    return true;
}

uint32_t Disk::getTotalBlocks() const {
    return totalBlocks_;
}

uint32_t Disk::getBlockSize() const {
    return blockSize_;
}
