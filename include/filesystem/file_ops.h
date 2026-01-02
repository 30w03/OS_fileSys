#ifndef FILE_OPS_H
#define FILE_OPS_H

#include "filesystem/block_manager.h"
#include "filesystem/directory_ops.h"
#include "filesystem/wal.h"
#include <string>
#include <vector>

class FileOps {
public:
    FileOps(BlockManager* blockManager, DirectoryOps* dirOps);
    
    void setWALManager(std::shared_ptr<WALManager> wal) { walManager_ = wal; }

    // 文件基础操作
    bool createFile(uint32_t userId, const std::string& path, mode_t mode = 0644);
    bool deleteFile(uint32_t userId, const std::string& path);
    bool fileExists(uint32_t userId, const std::string& path);
    
    // 文件读写
    ssize_t writeFile(uint32_t userId, const std::string& path, const char* data, size_t size, off_t offset = 0);
    ssize_t readFile(uint32_t userId, const std::string& path, char* buffer, size_t size, off_t offset = 0);
    
    // 文件信息
    size_t getFileSize(uint32_t userId, const std::string& path);
    bool truncate(uint32_t userId, const std::string& path, size_t newSize);
    
    // 文件元数据
    bool getFileInfo(uint32_t userId, const std::string& path, Inode& inode);
    bool setPermissions(uint32_t userId, const std::string& path, mode_t mode);
    
    // ACL 与 权限控制
    bool grantPermission(uint32_t userId, const std::string& path, uint32_t targetUid);
    bool revokePermission(uint32_t userId, const std::string& path, uint32_t targetUid);
    bool setFileLock(uint32_t userId, const std::string& path, bool locked);

private:
    BlockManager* blockManager_;
    DirectoryOps* dirOps_;
    std::shared_ptr<WALManager> walManager_;
    
    // 权限检查辅助函数
    enum class AccessMode { READ, WRITE, EXECUTE };
    bool checkPermission(uint32_t userId, const Inode& inode, AccessMode mode);

    // 辅助函数
    bool allocateBlocks(Inode& inode, size_t requiredBlocks);
    bool freeFileBlocks(Inode& inode);
    uint32_t getBlockNumber(const Inode& inode, size_t logicalBlock);
    bool setBlockNumber(Inode& inode, size_t logicalBlock, uint32_t physicalBlock);
};

#endif // FILE_OPS_H
