#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

namespace Config {
    // 磁盘配置
    constexpr uint32_t BLOCK_SIZE = 4096;
    constexpr uint32_t TOTAL_BLOCKS = 25600;
    constexpr uint64_t DISK_SIZE = static_cast<uint64_t>(BLOCK_SIZE) * TOTAL_BLOCKS;
    
    // Inode配置
    constexpr uint32_t INODE_SIZE = 128;
    constexpr uint32_t INODES_PER_BLOCK = BLOCK_SIZE / INODE_SIZE;
    constexpr uint32_t TOTAL_INODES = 10240;
    constexpr uint32_t INODE_BLOCKS = (TOTAL_INODES + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK;
    
    // 文件系统布局
    constexpr uint32_t SUPERBLOCK_BLOCK = 0;
    constexpr uint32_t INODE_BITMAP_BLOCK = 1;
    constexpr uint32_t DATA_BITMAP_BLOCK = 2;
    constexpr uint32_t INODE_TABLE_BLOCK = 3;
    constexpr uint32_t DATA_BLOCK_START = INODE_TABLE_BLOCK + INODE_BLOCKS;
    
    // 数据块配置
    constexpr uint32_t DATA_BLOCKS = TOTAL_BLOCKS - DATA_BLOCK_START;
    
    // 直接块数量
    constexpr uint32_t DIRECT_BLOCKS = 12;
    
    // 缓存配置
    constexpr uint32_t CACHE_SIZE = 128;
    
    // 文件名最大长度
    constexpr uint32_t MAX_FILENAME = 27;
    
    // 路径最大长度
    constexpr uint32_t MAX_PATH_LENGTH = 256;
}

#endif
