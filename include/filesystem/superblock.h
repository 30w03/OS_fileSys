#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include <cstdint>
#include <cstring>
#include <ctime>
#include <iostream>
#include <algorithm> // for std::max
#include "common/config.h"

// 设定 Superblock 结构体在内存中的固定大小（128 字节）
static const int SUPERBLOCK_STRUCT_SIZE = 128;

struct Superblock {
    // =========================================================
    // 1. 8字节对齐字段 (16 字节)
    // 放在最前面，防止编译器在 uint32 和 uint64 之间插入 padding
    // =========================================================
    uint64_t createTime;
    uint64_t mountTime;

    // =========================================================
    // 2. 4字节字段 (44 字节)
    // =========================================================
    uint32_t magic;
    uint32_t version;
    uint32_t blockSize;
    uint32_t totalBlocks;
    uint32_t totalInodes;
    uint32_t freeBlocks;
    uint32_t freeInodes;
    
    // 布局指针 (Block ID)
    uint32_t inodeBitmapBlock;
    uint32_t dataBitmapBlock;
    uint32_t inodeTableStart;
    uint32_t dataBlocksStart;

    // 日志区域 (WAL)
    uint32_t logStartBlock;
    uint32_t logSizeBlocks;

    // (目前已用: 16 + 44 + 8 = 68 字节)

    // =========================================================
    // 3. 填充字段 (60 字节)
    // 128 - 68 = 60
    // =========================================================
    uint8_t padding[SUPERBLOCK_STRUCT_SIZE - 68];

    // 构造函数：只做清零，不负责初始化逻辑
    Superblock() {
        std::memset(this, 0, sizeof(Superblock));
    }

    // 工厂函数：负责计算布局
    // 参数使用 Config 中的常量作为默认值
    static Superblock create(uint32_t total_blocks = Config::DEFAULT_TOTAL_BLOCKS, 
                             uint32_t total_inodes = Config::DEFAULT_TOTAL_INODES) {
        Superblock sb;
        std::memset(&sb, 0, sizeof(Superblock));

        sb.magic = 0x53465350; // Magic: "SFS0"
        sb.version = 1;
        sb.blockSize = Config::BLOCK_SIZE; // 写入磁盘的块大小
        sb.totalBlocks = total_blocks;
        sb.totalInodes = total_inodes;

        // --- 布局计算 ---
        
        // 0号块是 Superblock，从 1号块开始分配
        uint32_t currentBlock = Config::SUPERBLOCK_BLOCK_ID + 1; 

        // 1. Inode Bitmap
        // 计算需要多少个块来存放 Inode位图 (1 bit per inode)
        // 例如: 6400 inodes -> 6400 bits -> 800 bytes -> 1 block
        uint32_t bitsPerBlock = Config::BLOCK_SIZE * 8;
        uint32_t inodeBitmapBlocks = (sb.totalInodes + bitsPerBlock - 1) / bitsPerBlock;
        // 至少分配1个块，防止 totalInodes 为 0 的极端情况
        inodeBitmapBlocks = std::max(inodeBitmapBlocks, 1u); 
        
        sb.inodeBitmapBlock = currentBlock;
        currentBlock += inodeBitmapBlocks;

        // 2. Data Bitmap
        // 计算需要多少个块来存放 Data位图 (1 bit per block)
        uint32_t dataBitmapBlocks = (sb.totalBlocks + bitsPerBlock - 1) / bitsPerBlock;
        dataBitmapBlocks = std::max(dataBitmapBlocks, 1u);

        sb.dataBitmapBlock = currentBlock;
        currentBlock += dataBitmapBlocks;

        // 3. Inode Table
        sb.inodeTableStart = currentBlock;
        // 使用 Config 中算好的 INODES_PER_BLOCK
        uint32_t inodeTableBlocks = (sb.totalInodes + Config::INODES_PER_BLOCK - 1) / Config::INODES_PER_BLOCK;
        currentBlock += inodeTableBlocks;

        // 4. Log Area (WAL)
        sb.logStartBlock = currentBlock;
        sb.logSizeBlocks = 64; // 256KB Log Area
        currentBlock += sb.logSizeBlocks;

        // 5. Data Blocks
        sb.dataBlocksStart = currentBlock;
        
        // --- 剩余空间计算 ---
        if (sb.dataBlocksStart >= total_blocks) {
            sb.freeBlocks = 0;
            std::cerr << "[Superblock] Error: Metadata occupies all blocks! Disk too small." << std::endl;
        } else {
            sb.freeBlocks = total_blocks - sb.dataBlocksStart;
        }
        
        sb.freeInodes = sb.totalInodes;
        sb.createTime = static_cast<uint64_t>(std::time(nullptr));
        
        return sb;
    }

    bool isValid() const {
        return magic == 0x53465350 && version == 1;
    }

    void serialize(char* buffer) const {
        std::memcpy(buffer, this, sizeof(Superblock));
        // 将 buffer 剩余部分清零，确保不会把内存里的垃圾数据写入磁盘
        if (this->blockSize > sizeof(Superblock)) {
            std::memset(buffer + sizeof(Superblock), 0, this->blockSize - sizeof(Superblock));
        }
    }

    void deserialize(const char* buffer) {
        std::memcpy(this, buffer, sizeof(Superblock));
    }
};

// 编译时断言：确保大小严格为 128 字节
static_assert(sizeof(Superblock) == SUPERBLOCK_STRUCT_SIZE, "Superblock size mismatch! Check alignment.");

#endif