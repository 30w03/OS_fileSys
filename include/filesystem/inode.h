#ifndef INODE_H
#define INODE_H

#include <cstdint>
#include <ctime>

// 保持不变
const uint32_t MAX_DIRECT_BLOCKS = 12;
const uint32_t MAX_FILENAME_LEN = 255;
const uint32_t ROOT_INODE = 0; 
const uint32_t INVALID_INODE = 0xFFFFFFFF;
const uint32_t INVALID_BLOCK = 0xFFFFFFFF;
const uint32_t INODE_SIZE = 128; // 显式定义 Inode 大小

enum class FileType : uint8_t {
    UNUSED = 0,
    REGULAR = 1,
    DIRECTORY = 2,
    SYMLINK = 3
};

struct Inode {
    // --- 元数据区 (48 bytes) ---
    FileType type;              // [1 byte]  文件类型
    uint8_t  mode[3];           // [3 bytes] 权限 (rwx)，顺便填补 padding，或者用 uint16_t mode + 1 byte padding
    
    uint32_t size;              // [4 bytes] 文件大小
    uint32_t blocks;            // [4 bytes] 占用块数
    
    uint32_t uid;               // [4 bytes] 用户ID
    uint32_t gid;               // [4 bytes] 组ID
    uint32_t links;             // [4 bytes] 硬链接数
    uint32_t flags;             // [4 bytes] 预留标志位 (比如只读、系统文件等)
    uint32_t reserved1;         // [4 bytes] Padding for 8-byte alignment of atime

    // 时间戳必须定长！这里选用 uint64_t 避免 2038 问题
    uint64_t atime;             // [8 bytes] Access Time
    uint64_t mtime;             // [8 bytes] Modify Time
    uint64_t ctime;             // [8 bytes] Change Time

    // --- 数据指针区 (60 bytes) ---
    // 假设块号用 uint32_t (支持 4096 * 2^32 = 16TB 总空间)
    uint32_t directBlocks[MAX_DIRECT_BLOCKS]; // 12 * 4 = 48 bytes
    uint32_t indirectBlock;                   // 4 bytes
    uint32_t doubleIndirectBlock;             // 4 bytes
    uint32_t tripleIndirectBlock;             // 4 bytes

    // --- 填充区 (12 bytes) ---
    // 目前总计: 32 (元数据) + 24 (时间戳) + 60 (指针) = 116 bytes
    // 目标大小: 128 bytes
    // 需要填充: 12 bytes
    uint8_t padding[12]; 
};

// 编译时检查：确保 Inode 大小雷打不动是 128 字节
static_assert(sizeof(Inode) == 128, "Inode size must be strictly 128 bytes for disk alignment.");

#endif // INODE_H