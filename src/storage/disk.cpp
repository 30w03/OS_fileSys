#include "storage/disk.h"
#include <iostream>
#include <cstring>

Disk::Disk(const std::string& path) : path_(path) {
    if (!open()) {
        std::cout << "Disk file does not exist, will be created on format" << std::endl;
    } else {
        std::cout << "Disk loaded: " << path_ << std::endl;
    }
}

Disk::~Disk() {
    close();
}

bool Disk::open() {
    file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    return file_.is_open();
}

void Disk::close() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool Disk::format() {
    close();
    
    file_.open(path_, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file_.is_open()) {
        std::cerr << "Failed to create disk file" << std::endl;
        return false;
    }
    
    char zeroBlock[Config::BLOCK_SIZE];
    std::memset(zeroBlock, 0, Config::BLOCK_SIZE);
    
    for (uint32_t i = 0; i < Config::TOTAL_BLOCKS; i++) {
        file_.write(zeroBlock, Config::BLOCK_SIZE);
    }
    
    file_.close();
    
    if (!open()) {
        std::cerr << "Failed to reopen disk file" << std::endl;
        return false;
    }
    
    std::cout << "Disk formatted: " << Config::TOTAL_BLOCKS << " blocks, "
              << Config::DISK_SIZE / (1024 * 1024) << " MB" << std::endl;
    
    return true;
}

bool Disk::readBlock(uint32_t blockNum, char* buffer) {
    if (!file_.is_open()) {
        return false;
    }
    
    if (blockNum >= Config::TOTAL_BLOCKS) {
        return false;
    }
    
    file_.seekg(blockNum * Config::BLOCK_SIZE, std::ios::beg);
    file_.read(buffer, Config::BLOCK_SIZE);
    
    stats_.reads++;
    
    return file_.good();
}

bool Disk::writeBlock(uint32_t blockNum, const char* buffer) {
    if (!file_.is_open()) {
        return false;
    }
    
    if (blockNum >= Config::TOTAL_BLOCKS) {
        return false;
    }
    
    file_.seekp(blockNum * Config::BLOCK_SIZE, std::ios::beg);
    file_.write(buffer, Config::BLOCK_SIZE);
    file_.flush();
    
    stats_.writes++;
    
    return file_.good();
}

void Disk::printStats() const {
    std::cout << "Reads: " << stats_.reads << std::endl;
    std::cout << "Writes: " << stats_.writes << std::endl;
}
