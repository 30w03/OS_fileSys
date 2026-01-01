#pragma once

#include "storage/storage.h"
#include "protocol/protocol.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>

// 增加到 256 个块指针 (支持最大 1MB 文件)
static constexpr size_t MAX_BLOCKS_PER_FILE = 2560;

struct Inode {
    uint32_t id;
    uint32_t size;
    uint32_t timestamp;
    bool isDirectory;
    uint32_t blockPointers[MAX_BLOCKS_PER_FILE];  // 256 个块指针
    
    Inode() : id(0), size(0), timestamp(0), isDirectory(false) {
        for (size_t i = 0; i < MAX_BLOCKS_PER_FILE; i++) {
            blockPointers[i] = 0;
        }
    }
};

struct DirectoryEntry {
    char name[256];
    uint32_t inodeId;
};

class Filesystem {
public:
    explicit Filesystem(const std::string& diskImage);
    ~Filesystem();
    
    bool mount();
    void unmount();
    
    bool createFile(const std::string& path);
    bool deleteFile(const std::string& path);
    bool readFile(const std::string& path, std::vector<char>& data);
    bool writeFile(const std::string& path, const std::vector<char>& data);
    
    bool isFile(const std::string& path);
    bool isDirectory(const std::string& path);
    
    std::vector<FileListEntry> listFiles();
    std::vector<std::string> listDirectory(const std::string& path);
    
private:
    std::unique_ptr<Storage> storage_;
    std::map<uint32_t, Inode> inodeTable_;
    std::unordered_map<std::string, uint32_t> pathToInode_;
    bool mounted_;
    mutable std::mutex mutex_;
    
    uint32_t allocateInode();
    uint32_t allocateBlock();
    void freeInode(uint32_t id);
    void freeBlock(uint32_t blockId);
    
    bool loadInodeTable();
    bool saveInodeTable();
    
    Inode* getInode(uint32_t id);
    uint32_t findInode(const std::string& path);
};
