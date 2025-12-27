#include "filesystem/block_manager.h"
#include "storage/bitmap.h"
#include <iostream>
#include <cstring>

BlockManager::BlockManager(std::shared_ptr<Disk> disk, uint32_t cacheSize)
    : disk_(disk), cache_(std::make_unique<LRUCache>(cacheSize)) {
    memset(&superblock_, 0, sizeof(Superblock));
}

BlockManager::~BlockManager() {
    unmount();
}

bool BlockManager::format(uint32_t blockSize, uint32_t totalInodes) {
    if (!disk_) {
        std::cerr << "Disk not initialized" << std::endl;
        return false;
    }
    
    superblock_ = Superblock::create(disk_->getTotalBlocks(), totalInodes);
    
    // 1. 写入 Superblock (Block 0)
    char buffer[4096];
    memset(buffer, 0, 4096);
    memcpy(buffer, &superblock_, sizeof(Superblock));
    
    if (!disk_->writeBlock(0, buffer)) {
        std::cerr << "Failed to write superblock" << std::endl;
        return false;
    }
    
    // 2. 初始化并写入位图 (Block 1 & 2)
    // Inode Bitmap
    inodeBitmap_.assign(superblock_.totalInodes, 0);
    // 根目录 Inode (0) 预留
    inodeBitmap_[ROOT_INODE] = 1; 
    superblock_.freeInodes--;

    Bitmap inodeBm(superblock_.totalInodes);
    inodeBm.set(ROOT_INODE);
    memset(buffer, 0, 4096);
    inodeBm.serialize(buffer);
    if (!disk_->writeBlock(superblock_.inodeBitmapBlock, buffer)) {
        return false;
    }

    // Data Bitmap
    blockBitmap_.assign(superblock_.totalBlocks, 0);
    // 标记元数据区域为已使用
    // 0: Superblock
    // 1: Inode Bitmap
    // 2: Data Bitmap
    // 3...N: Inode Table
    for (uint32_t i = 0; i < superblock_.dataBlocksStart; i++) {
        blockBitmap_[i] = 1;
    }
    
    Bitmap blockBm(superblock_.totalBlocks);
    for (uint32_t i = 0; i < superblock_.dataBlocksStart; i++) {
        blockBm.set(i);
    }
    memset(buffer, 0, 4096);
    blockBm.serialize(buffer);
    if (!disk_->writeBlock(superblock_.dataBitmapBlock, buffer)) {
        return false;
    }

    // 3. 初始化 Inode Table
    memset(buffer, 0, 4096);
    for (uint32_t i = superblock_.inodeTableStart; i < superblock_.dataBlocksStart; i++) {
        disk_->writeBlock(i, buffer);
    }
    
    // 清空缓存
    cache_->clear();
    
    std::cout << "Filesystem formatted successfully" << std::endl;
    std::cout << "Total blocks: " << superblock_.totalBlocks << std::endl;
    std::cout << "Total inodes: " << superblock_.totalInodes << std::endl;
    std::cout << "Data blocks start: " << superblock_.dataBlocksStart << std::endl;
    
    return true;
}

bool BlockManager::mount() {
    if (!disk_) {
        std::cerr << "Disk not initialized" << std::endl;
        return false;
    }
    
    // 1. 读取 Superblock
    char buffer[4096];
    if (!disk_->readBlock(0, buffer)) {
        std::cerr << "Failed to read superblock" << std::endl;
        return false;
    }
    
    memcpy(&superblock_, buffer, sizeof(Superblock));
    
    if (superblock_.magic != SUPERBLOCK_MAGIC) {
        std::cerr << "Invalid filesystem magic number: " << std::hex << superblock_.magic << std::endl;
        return false;
    }
    
    // 2. 读取位图
    // Inode Bitmap
    if (!disk_->readBlock(superblock_.inodeBitmapBlock, buffer)) {
        return false;
    }
    Bitmap inodeBm(superblock_.totalInodes);
    inodeBm.deserialize(buffer);
    inodeBitmap_.resize(superblock_.totalInodes);
    for(uint32_t i=0; i<superblock_.totalInodes; i++) {
        inodeBitmap_[i] = inodeBm.test(i) ? 1 : 0;
    }

    // Data Bitmap
    if (!disk_->readBlock(superblock_.dataBitmapBlock, buffer)) {
        return false;
    }
    Bitmap blockBm(superblock_.totalBlocks);
    blockBm.deserialize(buffer);
    blockBitmap_.resize(superblock_.totalBlocks);
    for(uint32_t i=0; i<superblock_.totalBlocks; i++) {
        blockBitmap_[i] = blockBm.test(i) ? 1 : 0;
    }
    
    std::cout << "Filesystem mounted successfully" << std::endl;
    return true;
}

bool BlockManager::unmount() {
    if (!disk_) {
        return true;
    }
    
    // 1. 保存 Superblock
    char buffer[4096];
    memset(buffer, 0, 4096);
    memcpy(buffer, &superblock_, sizeof(Superblock));
    writeBlock(0, buffer);
    
    // 2. 保存位图
    // Inode Bitmap
    Bitmap inodeBm(superblock_.totalInodes);
    for(uint32_t i=0; i<superblock_.totalInodes; i++) {
        if(inodeBitmap_[i]) inodeBm.set(i);
    }
    memset(buffer, 0, 4096);
    inodeBm.serialize(buffer);
    writeBlock(superblock_.inodeBitmapBlock, buffer);

    // Data Bitmap
    Bitmap blockBm(superblock_.totalBlocks);
    for(uint32_t i=0; i<superblock_.totalBlocks; i++) {
        if(blockBitmap_[i]) blockBm.set(i);
    }
    memset(buffer, 0, 4096);
    blockBm.serialize(buffer);
    writeBlock(superblock_.dataBitmapBlock, buffer);

    // 清空缓存
    cache_->clear();
    
    return true;
}

uint32_t BlockManager::allocateInode() {
    for (uint32_t i = 0; i < superblock_.totalInodes; i++) {
        if (!inodeBitmap_[i]) {
            inodeBitmap_[i] = 1;
            superblock_.freeInodes--;
            
            // Persist Inode Bitmap
            // In a real FS, we'd only write the specific block of the bitmap.
            // Here, the bitmap is small enough to fit in one block.
            Bitmap bm(superblock_.totalInodes);
            for(size_t k=0; k<inodeBitmap_.size(); k++) {
                if(inodeBitmap_[k]) bm.set(k);
            }
            
            char buffer[4096];
            memset(buffer, 0, 4096);
            bm.serialize(buffer);
            if (!writeBlock(superblock_.inodeBitmapBlock, buffer)) {
                // Rollback on failure
                inodeBitmap_[i] = 0;
                superblock_.freeInodes++;
                return INVALID_INODE;
            }
            
            // Persist Superblock (counters)
            memset(buffer, 0, 4096);
            memcpy(buffer, &superblock_, sizeof(Superblock));
            writeBlock(0, buffer);
            
            return i;
        }
    }
    return INVALID_INODE;
}

bool BlockManager::freeInode(uint32_t inodeId) {
    if (inodeId >= superblock_.totalInodes) {
        return false;
    }
    
    if (!inodeBitmap_[inodeId]) {
        return false;
    }
    
    // 1. Clear Inode content on disk
    Inode inode;
    memset(&inode, 0, sizeof(Inode));
    inode.type = FileType::UNUSED;
    if (!writeInode(inodeId, inode)) {
        return false;
    }
    
    // 2. Update Bitmap in memory
    inodeBitmap_[inodeId] = 0;
    superblock_.freeInodes++;
    
    // 3. Persist Inode Bitmap
    Bitmap bm(superblock_.totalInodes);
    for(size_t k=0; k<inodeBitmap_.size(); k++) {
        if(inodeBitmap_[k]) bm.set(k);
    }
    
    char buffer[4096];
    memset(buffer, 0, 4096);
    bm.serialize(buffer);
    writeBlock(superblock_.inodeBitmapBlock, buffer);
    
    // 4. Persist Superblock
    memset(buffer, 0, 4096);
    memcpy(buffer, &superblock_, sizeof(Superblock));
    writeBlock(0, buffer);
    
    return true;
}

uint32_t BlockManager::allocateBlock() {
    for (uint32_t i = superblock_.dataBlocksStart; i < superblock_.totalBlocks; i++) {
        if (!blockBitmap_[i]) {
            blockBitmap_[i] = 1;
            superblock_.freeBlocks--;
            
            // Persist Data Bitmap
            Bitmap bm(superblock_.totalBlocks);
            for(size_t k=0; k<blockBitmap_.size(); k++) {
                if(blockBitmap_[k]) bm.set(k);
            }
            
            char buffer[4096];
            memset(buffer, 0, 4096);
            bm.serialize(buffer);
            if (!writeBlock(superblock_.dataBitmapBlock, buffer)) {
                blockBitmap_[i] = 0;
                superblock_.freeBlocks++;
                return INVALID_BLOCK;
            }
            
            // Persist Superblock
            memset(buffer, 0, 4096);
            memcpy(buffer, &superblock_, sizeof(Superblock));
            writeBlock(0, buffer);
            
            // Zero out the allocated block to prevent data leakage
            memset(buffer, 0, 4096);
            writeBlock(i, buffer);
            
            return i;
        }
    }
    return INVALID_BLOCK;
}

bool BlockManager::freeBlock(uint32_t blockId) {
    if (blockId >= superblock_.totalBlocks || blockId < superblock_.dataBlocksStart) {
        return false;
    }
    
    if (!blockBitmap_[blockId]) {
        return false;
    }
    
    blockBitmap_[blockId] = 0;
    superblock_.freeBlocks++;
    
    // Persist Data Bitmap
    Bitmap bm(superblock_.totalBlocks);
    for(size_t k=0; k<blockBitmap_.size(); k++) {
        if(blockBitmap_[k]) bm.set(k);
    }
    
    char buffer[4096];
    memset(buffer, 0, 4096);
    bm.serialize(buffer);
    writeBlock(superblock_.dataBitmapBlock, buffer);
    
    // Persist Superblock
    memset(buffer, 0, 4096);
    memcpy(buffer, &superblock_, sizeof(Superblock));
    writeBlock(0, buffer);
    
    return true;
}

bool BlockManager::readInode(uint32_t inodeId, Inode& inode) {
    if (inodeId >= superblock_.totalInodes) {
        return false;
    }
    
    uint32_t inodesPerBlock = superblock_.blockSize / sizeof(Inode);
    uint32_t blockId = superblock_.inodeTableStart + (inodeId / inodesPerBlock);
    uint32_t offset = (inodeId % inodesPerBlock) * sizeof(Inode);
    
    char buffer[4096];
    // 使用带缓存的 readBlock
    if (!readBlock(blockId, buffer)) {
        return false;
    }
    
    memcpy(&inode, buffer + offset, sizeof(Inode));
    return true;
}

bool BlockManager::writeInode(uint32_t inodeId, const Inode& inode) {
    if (inodeId >= superblock_.totalInodes) {
        return false;
    }
    
    uint32_t inodesPerBlock = superblock_.blockSize / sizeof(Inode);
    uint32_t blockId = superblock_.inodeTableStart + (inodeId / inodesPerBlock);
    uint32_t offset = (inodeId % inodesPerBlock) * sizeof(Inode);
    
    char buffer[4096];
    // 使用带缓存的 readBlock
    if (!readBlock(blockId, buffer)) {
        return false;
    }
    
    memcpy(buffer + offset, &inode, sizeof(Inode));
    
    // 使用带缓存的 writeBlock
    if (!writeBlock(blockId, buffer)) {
        return false;
    }
    
    return true;
}

bool BlockManager::readBlock(uint32_t blockId, char* buffer) {
    if (blockId >= superblock_.totalBlocks) {
        std::cerr << "Block ID out of range: " << blockId << std::endl;
        return false;
    }
    
    // 先查缓存
    if (cache_->get(blockId, buffer)) {
        return true;
    }
    
    // 缓存未命中，从磁盘读取
    if (!disk_->readBlock(blockId, buffer)) {
        return false;
    }
    
    // 放入缓存
    cache_->put(blockId, buffer);
    
    return true;
}

bool BlockManager::writeBlock(uint32_t blockId, const char* buffer) {
    if (blockId >= superblock_.totalBlocks) {
        std::cerr << "Block ID out of range: " << blockId << std::endl;
        return false;
    }
    
    // 写入磁盘
    if (!disk_->writeBlock(blockId, buffer)) {
        return false;
    }
    
    // 更新缓存
    cache_->put(blockId, buffer);
    
    return true;
}

void BlockManager::printStats() const {
    std::cout << "=== Block Manager Statistics ===" << std::endl;
    std::cout << "Total Blocks: " << superblock_.totalBlocks << std::endl;
    std::cout << "Block Size: " << superblock_.blockSize << std::endl;
    std::cout << "Free Blocks: " << superblock_.freeBlocks << std::endl;
    std::cout << "Total Inodes: " << superblock_.totalInodes << std::endl;
    std::cout << "Free Inodes: " << superblock_.freeInodes << std::endl;
    
    std::cout << "\n=== Cache Statistics ===" << std::endl;
    cache_->printStats();
}
