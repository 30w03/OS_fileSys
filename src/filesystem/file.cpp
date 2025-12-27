#include "filesystem/file.h"
#include <algorithm>
#include <cstring>
#include <iostream>

File::File(std::shared_ptr<BlockManager> bm, uint32_t inodeNum)
    : blockManager_(bm), inodeNum_(inodeNum), dirty_(false) {
    loadInode();
}

File::~File() {
    if (dirty_) {
        flush();
    }
}

void File::loadInode() {
    if (!blockManager_->readInode(inodeNum_, inode_)) {
        std::cerr << "Failed to load inode " << inodeNum_ << std::endl;
    }
}

void File::saveInode() {
    if (!blockManager_->writeInode(inodeNum_, inode_)) {
        std::cerr << "Failed to save inode " << inodeNum_ << std::endl;
    }
    dirty_ = false;
}

uint32_t File::getBlockNum(uint32_t logicalBlock) {
    if (logicalBlock < MAX_DIRECT_BLOCKS) {
        return inode_.directBlocks[logicalBlock];
    }
    return INVALID_BLOCK;
}

bool File::allocateBlock(uint32_t logicalBlock) {
    if (logicalBlock >= MAX_DIRECT_BLOCKS) {
        return false;
    }
    
    uint32_t physicalBlock = blockManager_->allocateBlock();
    if (physicalBlock == INVALID_BLOCK) {
        return false;
    }
    
    inode_.directBlocks[logicalBlock] = physicalBlock;
    inode_.blockCount++;
    dirty_ = true;
    
    std::cout << "Allocated block " << physicalBlock << " for logical block " << logicalBlock << std::endl;
    
    return true;
}

int32_t File::read(char* buffer, uint32_t size, uint32_t offset) {
    if (offset >= inode_.size) {
        return 0;
    }
    
    uint32_t bytesToRead = std::min(size, inode_.size - offset);
    uint32_t bytesRead = 0;
    
    std::cout << "Reading " << bytesToRead << " bytes from offset " << offset 
              << " (file size: " << inode_.size << ")" << std::endl;
    
    while (bytesRead < bytesToRead) {
        uint32_t blockIndex = (offset + bytesRead) / Config::BLOCK_SIZE;
        uint32_t blockOffset = (offset + bytesRead) % Config::BLOCK_SIZE;
        uint32_t remaining = bytesToRead - bytesRead;
        uint32_t toRead = std::min(remaining, Config::BLOCK_SIZE - blockOffset);
        
        uint32_t physicalBlock = getBlockNum(blockIndex);
        if (physicalBlock == INVALID_BLOCK || physicalBlock == 0) {
            std::cerr << "Invalid physical block " << physicalBlock 
                      << " for logical block " << blockIndex << std::endl;
            break;
        }
        
        std::cout << "Reading from physical block " << physicalBlock 
                  << " (logical " << blockIndex << ")" << std::endl;
        
        char blockBuffer[Config::BLOCK_SIZE];
        if (!blockManager_->readBlock(physicalBlock, blockBuffer)) {
            std::cerr << "Failed to read block " << physicalBlock << std::endl;
            break;
        }
        
        std::memcpy(buffer + bytesRead, blockBuffer + blockOffset, toRead);
        bytesRead += toRead;
    }
    
    inode_.accessTime = std::time(nullptr);
    dirty_ = true;
    
    return bytesRead;
}

int32_t File::write(const char* buffer, uint32_t size, uint32_t offset) {
    uint32_t bytesWritten = 0;
    
    std::cout << "Writing " << size << " bytes at offset " << offset << std::endl;
    
    while (bytesWritten < size) {
        uint32_t blockIndex = (offset + bytesWritten) / Config::BLOCK_SIZE;
        uint32_t blockOffset = (offset + bytesWritten) % Config::BLOCK_SIZE;
        uint32_t remaining = size - bytesWritten;
        uint32_t toWrite = std::min(remaining, Config::BLOCK_SIZE - blockOffset);
        
        uint32_t physicalBlock = getBlockNum(blockIndex);
        if (physicalBlock == INVALID_BLOCK || physicalBlock == 0) {
            if (!allocateBlock(blockIndex)) {
                std::cerr << "Failed to allocate block for logical block " << blockIndex << std::endl;
                break;
            }
            physicalBlock = inode_.directBlocks[blockIndex];
        }
        
        std::cout << "Writing to physical block " << physicalBlock 
                  << " (logical " << blockIndex << ")" << std::endl;
        
        char blockBuffer[Config::BLOCK_SIZE];
        
        if (blockOffset != 0 || toWrite != Config::BLOCK_SIZE) {
            if (!blockManager_->readBlock(physicalBlock, blockBuffer)) {
                std::memset(blockBuffer, 0, Config::BLOCK_SIZE);
            }
        }
        
        std::memcpy(blockBuffer + blockOffset, buffer + bytesWritten, toWrite);
        
        if (!blockManager_->writeBlock(physicalBlock, blockBuffer)) {
            std::cerr << "Failed to write block " << physicalBlock << std::endl;
            break;
        }
        
        bytesWritten += toWrite;
    }
    
    if (offset + bytesWritten > inode_.size) {
        inode_.size = offset + bytesWritten;
    }
    
    inode_.modifyTime = std::time(nullptr);
    dirty_ = true;
    
    // 立即保存 inode
    saveInode();
    
    return bytesWritten;
}

int32_t File::append(const char* buffer, uint32_t size) {
    return write(buffer, size, inode_.size);
}

bool File::truncate(uint32_t newSize) {
    if (newSize >= inode_.size) {
        inode_.size = newSize;
        dirty_ = true;
        return true;
    }
    
    uint32_t newBlockCount = (newSize + Config::BLOCK_SIZE - 1) / Config::BLOCK_SIZE;
    
    for (uint32_t i = newBlockCount; i < inode_.blockCount && i < MAX_DIRECT_BLOCKS; i++) {
        if (inode_.directBlocks[i] != 0) {
            blockManager_->freeBlock(inode_.directBlocks[i]);
            inode_.directBlocks[i] = 0;
        }
    }
    
    inode_.size = newSize;
    inode_.blockCount = newBlockCount;
    inode_.modifyTime = std::time(nullptr);
    dirty_ = true;
    
    return true;
}

uint32_t File::getSize() const {
    return inode_.size;
}

bool File::getInode(Inode& inode) const {
    inode = inode_;
    return true;
}

bool File::flush() {
    if (dirty_) {
        saveInode();
    }
    return true;
}