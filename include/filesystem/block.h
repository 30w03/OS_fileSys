#ifndef BLOCK_H
#define BLOCK_H

#include <memory>
#include <cstdint>
#include "storage/disk.h"
#include "storage/cache.h"
#include "storage/bitmap.h"
#include "filesystem/superblock.h"
#include "filesystem/inode.h"

class BlockManager {
public:
    BlockManager(std::shared_ptr<Disk> disk);
    ~BlockManager();
    
    bool format();
    bool mount();
    void unmount();
    
    bool readBlock(uint32_t blockNum, char* buffer);
    bool writeBlock(uint32_t blockNum, const char* buffer);
    
    bool readInode(uint32_t inodeNum, Inode& inode);
    bool writeInode(uint32_t inodeNum, const Inode& inode);
    
    int32_t allocateInode();
    bool freeInode(uint32_t inodeNum);
    
    int32_t allocateBlock();
    bool freeBlock(uint32_t blockNum);
    
    void printStats() const;
    
private:
    std::shared_ptr<Disk> disk_;
    std::unique_ptr<LRUCache> cache_;
    std::unique_ptr<Bitmap> inodeBitmap_;
    std::unique_ptr<Bitmap> dataBitmap_;
    Superblock superblock_;
    bool mounted_;
    
    bool saveBitmaps();
};

#endif