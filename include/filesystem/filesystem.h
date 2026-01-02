#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <string>
#include <memory>
#include <vector>
#include "filesystem/disk.h"
#include "filesystem/block_manager.h"
#include "filesystem/file_ops.h"
#include "filesystem/directory_ops.h"
#include "filesystem/wal.h"
#include "filesystem/snapshot.h"
#include "protocol/protocol.h" // For FileListEntry if needed

class Filesystem {
public:
    Filesystem(const std::string& diskImage);
    ~Filesystem();

    // 核心生命周期
    bool format(uint32_t blockSize = 4096, uint32_t totalInodes = 1024);
    bool mount();
    void unmount();

    // 获取功能模块
    std::shared_ptr<FileOps> getFileOps() { return fileOps_; }
    std::shared_ptr<DirectoryOps> getDirOps() { return dirOps_; }
    std::shared_ptr<BlockManager> getBlockManager() { return blockManager_; }
    std::shared_ptr<WALManager> getWALManager() { return walManager_; }
    std::shared_ptr<SnapshotManager> getSnapshotManager() { return snapshotManager_; }

    // --- 兼容旧接口 (适配器模式) ---
    // 注意：这些接口默认使用 Admin (uid=1) 权限，仅用于向后兼容
    bool createFile(const std::string& path);
    bool deleteFile(const std::string& path);
    bool exists(const std::string& path);
    
    // 读取文件全部内容
    bool readFile(const std::string& path, std::vector<char>& data);
    // 写入文件全部内容
    bool writeFile(const std::string& path, const std::vector<char>& data);

    // 列表
    std::vector<FileListEntry> listFiles();

private:
    std::string diskImage_;
    std::shared_ptr<Disk> disk_;
    std::shared_ptr<BlockManager> blockManager_;
    std::shared_ptr<DirectoryOps> dirOps_;
    std::shared_ptr<FileOps> fileOps_;
    std::shared_ptr<WALManager> walManager_;
    std::shared_ptr<SnapshotManager> snapshotManager_;
    bool mounted_;
};

#endif // FILESYSTEM_H
