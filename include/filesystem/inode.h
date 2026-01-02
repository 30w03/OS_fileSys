#ifndef INODE_H
#define INODE_H

#include <cstdint>
#include <ctime>

// 保持不变
const uint32_t MAX_DIRECT_BLOCKS = 10; // Reduced from 12 to make space for ACL
const uint32_t MAX_FILENAME_LEN = 255;
const uint32_t ROOT_INODE = 0; 
const uint32_t INVALID_INODE = 0xFFFFFFFF;
const uint32_t INVALID_BLOCK = 0xFFFFFFFF;
const uint32_t INODE_SIZE = 128; // 显式定义 Inode 大小

// Inode Flags
const uint32_t INODE_FLAG_LOCKED = 0x00000001; // 锁定文件（所有者只读）

enum class FileType : uint8_t {
    UNUSED = 0,
    REGULAR = 1,
    DIRECTORY = 2,
    SYMLINK = 3
};

struct Inode {
    // --- 元数据区 (32 bytes) ---
    FileType type;              // [1 byte]  文件类型
    uint8_t  mode[3];           // [3 bytes] 权限 (rwx)
    
    uint32_t size;              // [4 bytes] 文件大小
    uint32_t blocks;            // [4 bytes] 占用块数
    
    uint32_t uid;               // [4 bytes] 用户ID (Owner)
    uint32_t gid;               // [4 bytes] 组ID
    uint32_t links;             // [4 bytes] 硬链接数
    uint32_t flags;             // [4 bytes] 标志位 (如 INODE_FLAG_LOCKED)
    uint32_t reserved1;         // [4 bytes] Padding for 8-byte alignment of atime

    // --- 时间戳区 (24 bytes) ---
    uint64_t atime;             // [8 bytes] Access Time
    uint64_t mtime;             // [8 bytes] Modify Time
    uint64_t ctime;             // [8 bytes] Change Time

    // --- 数据指针区 (52 bytes) ---
    uint32_t directBlocks[MAX_DIRECT_BLOCKS]; // 10 * 4 = 40 bytes
    uint32_t indirectBlock;                   // 4 bytes
    uint32_t doubleIndirectBlock;             // 4 bytes
    uint32_t tripleIndirectBlock;             // 4 bytes

    // --- ACL 区 (16 bytes) ---
    uint32_t acl_uids[4];       // [16 bytes] 允许访问的额外用户ID列表 (只读权限)

    // --- 填充区 (4 bytes) ---
    uint32_t padding;           // [4 bytes] 补齐到 128 字节
};

static_assert(sizeof(Inode) == 128, "Inode size must be strictly 128 bytes for disk alignment.");

#endif // INODE_H