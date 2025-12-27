#ifndef BLOCK_MANAGER_H
#define BLOCK_MANAGER_H

#include "filesystem/disk.h"
#include "filesystem/inode.h"
#include "storage/cache.h"
#include <memory>
#include <vector>
#include <cstdint>

const uint32_t SUPERBLOCK_MAGIC = 0x12345678;
const uint32_t INODE_BLOCKS = 64;
const uint32_t INVALID_INODE = 0xFFFFFFFF;
const uint32_t INVALID_BLOCK = 0xFFFFFFFF;

struct SuperBlock {
    uint32_t magic;
    uint32_t blockSize;
    uint32_t totalBlocks;
    uint32_t inodeBlocks;
    uint32_t totalInodes;
    uint32_t firstDataBlock;
    uint32_t freeBlocks;
    uint32_t freeInodes;
};

class BlockManager {
public:
    explicit BlockManager(std::shared_ptr<Disk> disk, uint32_t cacheSize = 64);
    ~BlockManager();
    
    bool format();
    bool mount();
    bool unmount();
    
    uint32_t allocateInode();
    bool freeInode(uint32_t inodeId);
    
    uint32_t allocateBlock();
    bool freeBlock(uint32_t blockId);
    
    bool readInode(uint32_t inodeId, Inode& inode);
    bool writeInode(uint32_t inodeId, const Inode& inode);
    
    bool readBlock(uint32_t blockId, char* buffer);
    bool writeBlock(uint32_t blockId, const char* buffer);
    
    void printStats() const;
    
private:
    std::shared_ptr<Disk> disk_;
    SuperBlock superblock_;
    std::vector<bool> inodeBitmap_;
    std::vector<bool> blockBitmap_;
    std::unique_ptr<LRUCache> cache_;  // LRU 缓存
};

#endif // BLOCK_MANAGER_H
