#ifndef DIRECTORY_OPS_H
#define DIRECTORY_OPS_H

#include "filesystem/block_manager.h"
#include "filesystem/directory.h"
#include "filesystem/inode.h"
#include "filesystem/wal.h"
#include <string>
#include <vector>
#include <memory>

class DirectoryOps {
public:
    explicit DirectoryOps(BlockManager* blockManager);
    ~DirectoryOps() = default;
    
    void setWALManager(std::shared_ptr<WALManager> wal) { walManager_ = wal; }

    // 初始化根目录
    bool initializeRoot();
    
    // 创建目录（支持递归创建）
    bool mkdir(const std::string& path, bool recursive = false);
    
    // 删除目录（必须为空）
    bool rmdir(const std::string& path);
    
    // 列出目录内容
    std::vector<DirectoryEntry> listDirectory(uint32_t inodeId);
    
    // 在目录中查找文件/目录
    uint32_t lookup(uint32_t dirInode, const std::string& name);
    
    // 在目录中添加条目
    bool addEntry(uint32_t dirInode, const std::string& name, uint32_t inodeId);
    
    // 在目录中删除条目
    bool removeEntry(uint32_t dirInode, const std::string& name);

    // Update entry (for CoW)
    bool updateEntry(uint32_t dirInode, const std::string& name, uint32_t newInodeId);
    
    // Check if entry is in shared block
    bool isEntryInSharedBlock(uint32_t dirInode, const std::string& name);

    // 路径解析：返回文件的 inode 编号
    uint32_t resolvePath(const std::string& path);
    
    // 检查目录是否为空
    bool isDirectoryEmpty(uint32_t inodeId);
    
    // 分割路径
    static std::vector<std::string> splitPath(const std::string& path);
    
private:
    BlockManager* blockManager_;
    std::shared_ptr<WALManager> walManager_;
    
    // 创建单个目录（不递归）
    bool createSingleDirectory(const std::string& path);
    
    // 读取目录内容
    bool readDirectoryBlock(uint32_t blockId, std::vector<DirectoryEntry>& entries);
    
    // 写入目录内容
    bool writeDirectoryBlock(uint32_t blockId, const std::vector<DirectoryEntry>& entries);

    // Deep copy a directory block (copy block + copy all inodes inside)
    uint32_t copyDirectoryBlock(uint32_t blockId);
};

#endif // DIRECTORY_OPS_H
