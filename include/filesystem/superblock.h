#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include <cstdint>
#include <cstring>
#include <ctime>
#include "common/config.h"

struct Superblock {
    uint32_t magic;
    uint32_t version;
    uint32_t blockSize;
    uint32_t totalBlocks;
    uint32_t totalInodes;
    uint32_t freeBlocks;
    uint32_t freeInodes;
    uint32_t inodeTableStart;
    uint32_t dataBlocksStart;
    uint32_t inodeBitmapBlock;
    uint32_t dataBitmapBlock;
    time_t createTime;
    time_t mountTime;
    
    Superblock() {
        magic = 0x53465350;
        version = 1;
        blockSize = Config::BLOCK_SIZE;
        totalBlocks = Config::TOTAL_BLOCKS;
        totalInodes = Config::TOTAL_INODES;
        freeBlocks = Config::TOTAL_BLOCKS - Config::DATA_BLOCK_START;
        freeInodes = Config::TOTAL_INODES;
        createTime = std::time(nullptr);
        mountTime = 0;
        inodeTableStart = Config::INODE_TABLE_BLOCK;
        dataBlocksStart = Config::DATA_BLOCK_START;
        inodeBitmapBlock = Config::INODE_BITMAP_BLOCK;
        dataBitmapBlock = Config::DATA_BITMAP_BLOCK;
    }
    
    bool isValid() const {
        return magic == 0x53465350 && version == 1;
    }
    
    void serialize(char* buffer) const {
        std::memcpy(buffer, this, sizeof(Superblock));
    }
    
    void deserialize(const char* buffer) {
        std::memcpy(this, buffer, sizeof(Superblock));
    }
};

#endif
