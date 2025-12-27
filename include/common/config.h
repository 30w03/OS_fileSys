#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

namespace Config {
    // ==========================================
    // 1. 物理/核心参数 (一旦定下，极少修改)
    // ==========================================
    
    // 块大小 (4KB)
    constexpr uint32_t BLOCK_SIZE = 4096;
    
    // Inode 大小 (128字节)
    constexpr uint32_t INODE_SIZE = 128;
    
    // 一个块能存多少个 Inode (4096 / 128 = 32)
    constexpr uint32_t INODES_PER_BLOCK = BLOCK_SIZE / INODE_SIZE;

    // 直接索引块数量 (Ext2 标准通常是 12)
    constexpr uint32_t DIRECT_BLOCKS = 12;

    // 文件名最大长度 (27 + 1 null + 4 inode_id = 32字节对齐，很完美)
    constexpr uint32_t MAX_FILENAME = 27;
    
    // 路径最大长度
    constexpr uint32_t MAX_PATH_LENGTH = 256;

    // ==========================================
    // 2. 默认格式化参数 (可以被命令行参数覆盖)
    // ==========================================
    
    // 默认磁盘总大小 (约 100MB)
    constexpr uint32_t DEFAULT_TOTAL_BLOCKS = 25600;
    
    // 默认 Inode 总数 (通常是总块数的 1/4 或 1/3)
    constexpr uint32_t DEFAULT_TOTAL_INODES = DEFAULT_TOTAL_BLOCKS / 4;

    // ==========================================
    // 3. 绝对固定的位置 (仅限 Superblock)
    // ==========================================
    
    // Superblock 永远在第 0 块，这是雷打不动的
    constexpr uint32_t SUPERBLOCK_BLOCK_ID = 0;

    // ------------------------------------------
    // 警告：不要在这里定义 DATA_BLOCK_START 等位置！
    // 这些位置应该从 Superblock 结构体中读取。
    // ------------------------------------------
}

#endif