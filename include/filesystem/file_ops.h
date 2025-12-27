#ifndef FILE_OPS_H
#define FILE_OPS_H

#include "filesystem/block_manager.h"
#include "filesystem/directory_ops.h"
#include <string>
#include <vector>

class FileOps {
public:
    FileOps(BlockManager* blockManager, DirectoryOps* dirOps);
    
    // 文件基础操作
    bool createFile(const std::string& path, mode_t mode = 0644);
    bool deleteFile(const std::string& path);
    bool fileExists(const std::string& path);
    
    // 文件读写
    ssize_t writeFile(const std::string& path, const char* data, size_t size, off_t offset = 0);
    ssize_t readFile(const std::string& path, char* buffer, size_t size, off_t offset = 0);
    
    // 文件信息
    size_t getFileSize(const std::string& path);
    bool truncate(const std::string& path, size_t newSize);
    
    // 文件元数据
    bool getFileInfo(const std::string& path, Inode& inode);
    bool setPermissions(const std::string& path, mode_t mode);
    
private:
    BlockManager* blockManager_;
    DirectoryOps* dirOps_;
    
    // 辅助函数
    bool allocateBlocks(Inode& inode, size_t requiredBlocks);
    bool freeFileBlocks(Inode& inode);
    uint32_t getBlockNumber(const Inode& inode, size_t logicalBlock);
    bool setBlockNumber(Inode& inode, size_t logicalBlock, uint32_t physicalBlock);
};

#endif // FILE_OPS_H
