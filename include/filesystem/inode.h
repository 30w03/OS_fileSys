#ifndef INODE_H
#define INODE_H

#include <cstdint>
#include <ctime>

const uint32_t MAX_DIRECT_BLOCKS = 12;
const uint32_t MAX_FILENAME_LEN = 255;
const uint32_t ROOT_INODE = 0;  // 根目录的 inode 编号

enum class FileType : uint8_t {
    UNUSED = 0,
    REGULAR = 1,
    DIRECTORY = 2,
    SYMLINK = 3
};

struct Inode {
    FileType type;                              // 文件类型
    uint8_t padding1[3];                        // 对齐填充
    uint32_t size;                              // 文件大小（字节）
    uint32_t blocks;                            // 占用的块数
    uint32_t links;                             // 硬链接计数
    uint32_t uid;                               // 用户ID
    uint32_t gid;                               // 组ID
    time_t atime;                               // 最后访问时间
    time_t mtime;                               // 最后修改时间
    time_t ctime;                               // 状态改变时间
    uint32_t directBlocks[MAX_DIRECT_BLOCKS];   // 直接块指针
    uint32_t indirectBlock;                     // 一级间接块指针
    uint32_t doubleIndirectBlock;               // 二级间接块指针
    uint32_t tripleIndirectBlock;               // 三级间接块指针
};

#endif // INODE_H
