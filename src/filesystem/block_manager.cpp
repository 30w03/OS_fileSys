#include "filesystem/block_manager.h"
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
    
    // 写入超级块
    char buffer[4096];
    memset(buffer, 0, 4096);
    memcpy(buffer, &superblock_, sizeof(Superblock));
    
    if (!disk_->writeBlock(0, buffer)) {
        std::cerr << "Failed to write superblock" << std::endl;
        return false;
    }
    
    // 初始化元数据区域
    memset(buffer, 0, 4096);
    for (uint32_t i = 1; i < superblock_.dataBlocksStart; i++) {
        disk_->writeBlock(i, buffer);
    }
    
    // 初始化位图
    inodeBitmap_.assign(superblock_.totalInodes, 0);
    blockBitmap_.assign(superblock_.totalBlocks, 0);
    
    // 标记已使用的块
    for (uint32_t i = 0; i < superblock_.dataBlocksStart; i++) {
        blockBitmap_[i] = 1;
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
    
    // 直接从磁盘读取超级块
    char buffer[4096];
    if (!disk_->readBlock(0, buffer)) {
        std::cerr << "Failed to read superblock" << std::endl;
        return false;
    }
    
    memcpy(&superblock_, buffer, sizeof(Superblock));
    
    // 验证魔数
    if (superblock_.magic != 0x53465350) {
        std::cerr << "Invalid filesystem magic number" << std::endl;
        return false;
    }
    
    // 将超级块放入缓存
    cache_->put(0, buffer);
    
    // 重建位图
    inodeBitmap_.assign(superblock_.totalInodes, 0);
    blockBitmap_.assign(superblock_.totalBlocks, 0);
    
    // 标记已使用的块
    for (uint32_t i = 0; i < superblock_.dataBlocksStart; i++) {
        blockBitmap_[i] = 1;
    }
    
    // 扫描 inode 表，重建位图
    for (uint32_t i = 0; i < superblock_.totalInodes; i++) {
        Inode inode;
        if (readInode(i, inode) && inode.type != FileType::UNUSED) {
            inodeBitmap_[i] = 1;
            superblock_.freeInodes--;
            
            // 标记使用的数据块
            for (uint32_t j = 0; j < MAX_DIRECT_BLOCKS && inode.directBlocks[j] != 0; j++) {
                if (inode.directBlocks[j] < superblock_.totalBlocks) {
                    if (!blockBitmap_[inode.directBlocks[j]]) {
                        blockBitmap_[inode.directBlocks[j]] = 1;
                        superblock_.freeBlocks--;
                    }
                }
            }
        }
    }
    
    std::cout << "Filesystem mounted successfully" << std::endl;
    return true;
}

bool BlockManager::unmount() {
    if (!disk_) {
        return true;
    }
    
    // 更新超级块
    char buffer[4096];
    memset(buffer, 0, 4096);
    memcpy(buffer, &superblock_, sizeof(Superblock));
    writeBlock(0, buffer);
    
    // 清空缓存
    cache_->clear();
    
    return true;
}

uint32_t BlockManager::allocateInode() {
    for (uint32_t i = 0; i < superblock_.totalInodes; i++) {
        if (!inodeBitmap_[i]) {
            inodeBitmap_[i] = 1;
            superblock_.freeInodes--;
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
    
    Inode inode;
    memset(&inode, 0, sizeof(Inode));
    inode.type = FileType::UNUSED;
    writeInode(inodeId, inode);
    
    inodeBitmap_[inodeId] = 0;
    superblock_.freeInodes++;
    return true;
}

uint32_t BlockManager::allocateBlock() {
    for (uint32_t i = superblock_.dataBlocksStart; i < superblock_.totalBlocks; i++) {
        if (!blockBitmap_[i]) {
            blockBitmap_[i] = 1;
            superblock_.freeBlocks--;
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
