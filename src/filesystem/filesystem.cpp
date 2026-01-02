#include "filesystem/filesystem.h"
#include <iostream>
#include <cstring>

Filesystem::Filesystem(const std::string& diskImage)
    : diskImage_(diskImage), mounted_(false) {
    disk_ = std::make_shared<Disk>(diskImage);
    blockManager_ = std::make_shared<BlockManager>(disk_, 1024);
    dirOps_ = std::make_shared<DirectoryOps>(blockManager_.get());
    fileOps_ = std::make_shared<FileOps>(blockManager_.get(), dirOps_.get());
    snapshotManager_ = std::make_shared<SnapshotManager>(blockManager_);
}

Filesystem::~Filesystem() {
    unmount();
}

bool Filesystem::format(uint32_t blockSize, uint32_t totalInodes) {
    // 默认 100MB
    uint32_t totalBlocks = (100 * 1024 * 1024) / blockSize;
    if (!disk_->create(totalBlocks, blockSize)) {
        return false;
    }
    if (!disk_->open()) return false;

    return blockManager_->format(blockSize, totalInodes);
}

bool Filesystem::mount() {
    if (mounted_) return true;
    
    if (!disk_->open()) {
        // 如果打开失败，尝试格式化
        std::cout << "Disk not found, formatting..." << std::endl;
        if (!format()) return false;
    }
    
    if (!blockManager_->mount()) {
        return false;
    }
    
    // 初始化 WAL
    walManager_ = std::make_shared<WALManager>(blockManager_, 
                                             blockManager_->getSuperblock().logStartBlock, 
                                             blockManager_->getSuperblock().logSizeBlocks);
    walManager_->init();
    
    // 恢复日志
    walManager_->recover(fileOps_, dirOps_);

    // 注入 WAL 到 FileOps 和 DirectoryOps (需要修改 FileOps/DirOps 接口)
    fileOps_->setWALManager(walManager_);
    dirOps_->setWALManager(walManager_);

    mounted_ = true;
    return true;
}

void Filesystem::unmount() {
    if (!mounted_) return;
    blockManager_->unmount();
    disk_->close();
    mounted_ = false;
}

// --- 兼容接口 ---

bool Filesystem::createFile(const std::string& path) {
    // 默认使用 Admin (uid=1)
    return fileOps_->createFile(1, path);
}

bool Filesystem::deleteFile(const std::string& path) {
    return fileOps_->deleteFile(1, path);
}

bool Filesystem::exists(const std::string& path) {
    return fileOps_->fileExists(1, path);
}

bool Filesystem::readFile(const std::string& path, std::vector<char>& data) {
    size_t size = fileOps_->getFileSize(1, path);
    if (size == 0) {
        // 可能是空文件，也可能是不存在
        if (!fileOps_->fileExists(1, path)) return false;
        data.clear();
        return true;
    }
    
    data.resize(size);
    ssize_t bytesRead = fileOps_->readFile(1, path, data.data(), size);
    if (bytesRead < 0) return false;
    
    data.resize(bytesRead);
    return true;
}

bool Filesystem::writeFile(const std::string& path, const std::vector<char>& data) {
    // 如果文件不存在，先创建
    if (!fileOps_->fileExists(1, path)) {
        if (!fileOps_->createFile(1, path)) return false;
    }
    
    // 覆盖写入：先截断
    fileOps_->truncate(1, path, 0);
    
    ssize_t bytesWritten = fileOps_->writeFile(1, path, data.data(), data.size());
    return bytesWritten == (ssize_t)data.size();
}

std::vector<FileListEntry> Filesystem::listFiles() {
    // 这是一个简化实现，只列出根目录下的文件
    std::vector<FileListEntry> entries;
    auto dirEntries = dirOps_->listDirectory(ROOT_INODE);
    
    for (const auto& dirEntry : dirEntries) {
        std::string name(dirEntry.name);
        if (name == "." || name == "..") continue;
        
        std::string path = "/" + name;
        Inode inode;
        if (fileOps_->getFileInfo(1, path, inode)) {
            if (inode.type == FileType::REGULAR) {
                FileListEntry entry;
                entry.filename = path;
                entry.size = inode.size;
                entry.timestamp = inode.mtime;
                entries.push_back(entry);
            }
        }
    }
    return entries;
}
