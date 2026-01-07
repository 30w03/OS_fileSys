#include "filesystem/block_manager.h"
#include "storage/bitmap.h"
#include <iostream>
#include <cstring>
#include <fstream>

// 构造函数：初始化 BlockManager，设置磁盘对象和缓存大小
BlockManager::BlockManager(std::shared_ptr<Disk> disk, uint32_t cacheSize)
    : disk_(disk), cache_(std::make_unique<LRUCache>(cacheSize)) {
    // 初始化 Superblock 为全零
    memset(&superblock_, 0, sizeof(Superblock));
}

// 析构函数：卸载文件系统，确保所有元数据刷回磁盘
BlockManager::~BlockManager() {
    unmount();
}

// 格式化磁盘：初始化 Superblock、位图和 Inode 表
bool BlockManager::format(uint32_t blockSize, uint32_t totalInodes) {
    if (!disk_) {
        std::cerr << "Disk not initialized" << std::endl;
        return false;
    }
    
    // 创建 Superblock，设置磁盘块数和 Inode 数量
    superblock_ = Superblock::create(disk_->getTotalBlocks(), totalInodes);
    
    // 写入 Superblock 到磁盘的第 0 块
    char buffer[4096];
    memset(buffer, 0, 4096);
    memcpy(buffer, &superblock_, sizeof(Superblock));
    
    if (!disk_->writeBlock(0, buffer)) {
        std::cerr << "Failed to write superblock" << std::endl;
        return false;
    }
    
    // 初始化并写入 Inode 位图
    inodeBitmap_.assign(superblock_.totalInodes, 0);
    inodeBitmap_[ROOT_INODE] = 1; // 根目录 Inode 预留
    superblock_.freeInodes--;

    Bitmap inodeBm(superblock_.totalInodes);
    inodeBm.set(ROOT_INODE);
    memset(buffer, 0, 4096);
    inodeBm.serialize(buffer);
    if (!disk_->writeBlock(superblock_.inodeBitmapBlock, buffer)) {
        return false;
    }

    // 初始化并写入数据块位图
    blockBitmap_.assign(superblock_.totalBlocks, 0);
    for (uint32_t i = 0; i < superblock_.dataBlocksStart; i++) {
        blockBitmap_[i] = 1; // 标记元数据区域(包括日志)为已使用
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

    // 初始化 Inode 表，将其清零
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
    
    // 创建根目录 Inode
    Inode rootInode;
    memset(&rootInode, 0, sizeof(Inode));
    rootInode.type = FileType::DIRECTORY;
    rootInode.size = 0;
    rootInode.blocks = 0;
    rootInode.links = 2;
    rootInode.uid = 0;
    rootInode.gid = 0;
    rootInode.atime = rootInode.mtime = rootInode.ctime = std::time(nullptr);
    
    if (!writeInode(ROOT_INODE, rootInode)) {
        std::cerr << "Failed to write root inode" << std::endl;
        return false;
    }

    return true;
}

// 挂载文件系统：从磁盘加载 Superblock 和位图到内存
bool BlockManager::mount() {
    if (!disk_) {
        std::cerr << "Disk not initialized" << std::endl;
        return false;
    }
    
    // 读取 Superblock
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
    
    // 读取 Inode 位图
    if (!disk_->readBlock(superblock_.inodeBitmapBlock, buffer)) {
        return false;
    }
    Bitmap inodeBm(superblock_.totalInodes);
    inodeBm.deserialize(buffer);
    inodeBitmap_.resize(superblock_.totalInodes);
    for(uint32_t i=0; i<superblock_.totalInodes; i++) {
        inodeBitmap_[i] = inodeBm.test(i) ? 1 : 0;
    }

    // 读取数据块位图
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
    
    loadRefCounts();
    return true;
}

// 卸载文件系统：将内存中的元数据刷回磁盘
bool BlockManager::unmount() {
    if (!disk_) {
        return true;
    }
    
    // 保存 Superblock
    char buffer[4096];
    memset(buffer, 0, 4096);
    memcpy(buffer, &superblock_, sizeof(Superblock));
    writeBlock(0, buffer);
    
    // 保存 Inode 位图
    Bitmap inodeBm(superblock_.totalInodes);
    for(uint32_t i=0; i<superblock_.totalInodes; i++) {
        if(inodeBitmap_[i]) inodeBm.set(i);
    }
    memset(buffer, 0, 4096);
    inodeBm.serialize(buffer);
    writeBlock(superblock_.inodeBitmapBlock, buffer);

    // 保存数据块位图
    Bitmap blockBm(superblock_.totalBlocks);
    for(uint32_t i=0; i<superblock_.totalBlocks; i++) {
        if(blockBitmap_[i]) blockBm.set(i);
    }
    memset(buffer, 0, 4096);
    blockBm.serialize(buffer);
    writeBlock(superblock_.dataBitmapBlock, buffer);

    // 清空缓存
    cache_->clear();
    
    saveRefCounts();
    return true;
}

// 同步：强制将系统状态写入磁盘
bool BlockManager::sync() {
    if (!disk_) {
        return false;
    }

    // 保存 Superblock
    char buffer[4096];
    memset(buffer, 0, 4096);
    memcpy(buffer, &superblock_, sizeof(Superblock));
    writeBlock(0, buffer);

    // 保存 Inode 位图
    Bitmap inodeBm(superblock_.totalInodes);
    for(uint32_t i=0; i<superblock_.totalInodes; i++) {
        if(inodeBitmap_[i]) inodeBm.set(i);
    }
    memset(buffer, 0, 4096);
    inodeBm.serialize(buffer);
    writeBlock(superblock_.inodeBitmapBlock, buffer);

    // 保存数据块位图
    Bitmap blockBm(superblock_.totalBlocks);
    for(uint32_t i=0; i<superblock_.totalBlocks; i++) {
        if(blockBitmap_[i]) blockBm.set(i);
    }
    memset(buffer, 0, 4096);
    blockBm.serialize(buffer);
    writeBlock(superblock_.dataBitmapBlock, buffer);

    saveRefCounts();
    return true;
}

// 分配一个空闲的 Inode，返回其编号
uint32_t BlockManager::allocateInode() {
    for (uint32_t i = 0; i < superblock_.totalInodes; i++) {
        if (!inodeBitmap_[i]) {
            inodeBitmap_[i] = 1;
            superblock_.freeInodes--;
            
            // 持久化 Inode 位图
            Bitmap bm(superblock_.totalInodes);
            for(size_t k=0; k<inodeBitmap_.size(); k++) {
                if(inodeBitmap_[k]) bm.set(k);
            }
            
            char buffer[4096];
            memset(buffer, 0, 4096);
            bm.serialize(buffer);
            if (!writeBlock(superblock_.inodeBitmapBlock, buffer)) {
                // 如果失败，回滚
                inodeBitmap_[i] = 0;
                superblock_.freeInodes++;
                return INVALID_INODE;
            }
            
            // 持久化 Superblock
            memset(buffer, 0, 4096);
            memcpy(buffer, &superblock_, sizeof(Superblock));
            writeBlock(0, buffer);
            
            return i;
        }
    }
    return INVALID_INODE;
}

bool BlockManager::forceAllocateInode(uint32_t inodeId) {
    if (inodeId >= superblock_.totalInodes) return false;
    if (inodeBitmap_[inodeId]) return true; // Already allocated

    inodeBitmap_[inodeId] = 1;
    superblock_.freeInodes--;

    // Persist Bitmap
    Bitmap bm(superblock_.totalInodes);
    for(size_t k=0; k<inodeBitmap_.size(); k++) {
        if(inodeBitmap_[k]) bm.set(k);
    }
    char buffer[4096];
    memset(buffer, 0, 4096);
    bm.serialize(buffer);
    writeBlock(superblock_.inodeBitmapBlock, buffer);

    // Persist Superblock
    memset(buffer, 0, 4096);
    memcpy(buffer, &superblock_, sizeof(Superblock));
    writeBlock(0, buffer);

    return true;
}

// 释放一个 Inode
bool BlockManager::freeInode(uint32_t inodeId) {
    if (inodeId >= superblock_.totalInodes) {
        return false;
    }
    
    if (!inodeBitmap_[inodeId]) {
        return false;
    }
    
    // 清空磁盘上的 Inode 内容
    Inode inode;
    memset(&inode, 0, sizeof(Inode));
    inode.type = FileType::UNUSED;
    if (!writeInode(inodeId, inode)) {
        return false;
    }
    
    // 更新内存中的位图
    inodeBitmap_[inodeId] = 0;
    superblock_.freeInodes++;
    
    // 持久化 Inode 位图
    Bitmap bm(superblock_.totalInodes);
    for(size_t k=0; k<inodeBitmap_.size(); k++) {
        if(inodeBitmap_[k]) bm.set(k);
    }
    
    char buffer[4096];
    memset(buffer, 0, 4096);
    bm.serialize(buffer);
    writeBlock(superblock_.inodeBitmapBlock, buffer);
    
    // 持久化 Superblock
    memset(buffer, 0, 4096);
    memcpy(buffer, &superblock_, sizeof(Superblock));
    writeBlock(0, buffer);
    
    return true;
}

// 分配一个空闲的数据块，返回其编号
uint32_t BlockManager::allocateBlock() {
    for (uint32_t i = superblock_.dataBlocksStart; i < superblock_.totalBlocks; i++) {
        if (!blockBitmap_[i]) {
            blockBitmap_[i] = 1;
            superblock_.freeBlocks--;
            
            // 持久化数据块位图
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
            
            // 持久化 Superblock
            memset(buffer, 0, 4096);
            memcpy(buffer, &superblock_, sizeof(Superblock));
            writeBlock(0, buffer);
            
            // 清空分配的数据块，防止数据泄露
            memset(buffer, 0, 4096);
            writeBlock(i, buffer);
            
            // Set refcount to 1
            refCounts_[i] = 1;
            
            return i;
        }
    }
    return INVALID_BLOCK;
}

// 释放一个数据块
bool BlockManager::freeBlock(uint32_t blockId) {
    if (blockId >= superblock_.totalBlocks || blockId < superblock_.dataBlocksStart) {
    // Decrement refcount
    decRef(blockId);
    
    // Only free if refcount is 0
    if (getRef(blockId) > 0) {
        return true; // Still used by others
    }

        return false;
    }
    
    if (!blockBitmap_[blockId]) {
        return false;
    }
    
    blockBitmap_[blockId] = 0;
    superblock_.freeBlocks++;
    
    // 持久化数据块位图
    Bitmap bm(superblock_.totalBlocks);
    for(size_t k=0; k<blockBitmap_.size(); k++) {
        if(blockBitmap_[k]) bm.set(k);
    }
    
    char buffer[4096];
    memset(buffer, 0, 4096);
    bm.serialize(buffer);
    writeBlock(superblock_.dataBitmapBlock, buffer);
    
    // 持久化 Superblock
    memset(buffer, 0, 4096);
    memcpy(buffer, &superblock_, sizeof(Superblock));
    writeBlock(0, buffer);
    
    return true;
}

// 从磁盘读取一个 Inode
bool BlockManager::readInode(uint32_t inodeId, Inode& inode) {
    if (inodeId >= superblock_.totalInodes) {
        return false;
    }
    
    uint32_t inodesPerBlock = superblock_.blockSize / sizeof(Inode);
    uint32_t blockId = superblock_.inodeTableStart + (inodeId / inodesPerBlock);
    uint32_t offset = (inodeId % inodesPerBlock) * sizeof(Inode);
    
    char buffer[4096];
    if (!readBlock(blockId, buffer)) {
        return false;
    }
    
    memcpy(&inode, buffer + offset, sizeof(Inode));
    return true;
}

// 将一个 Inode 写入磁盘
bool BlockManager::writeInode(uint32_t inodeId, const Inode& inode) {
    if (inodeId >= superblock_.totalInodes) {
        return false;
    }
    
    uint32_t inodesPerBlock = superblock_.blockSize / sizeof(Inode);
    uint32_t blockId = superblock_.inodeTableStart + (inodeId / inodesPerBlock);
    uint32_t offset = (inodeId % inodesPerBlock) * sizeof(Inode);
    
    char buffer[4096];
    if (!readBlock(blockId, buffer)) {
        return false;
    }
    
    memcpy(buffer + offset, &inode, sizeof(Inode));
    
    if (!writeBlock(blockId, buffer)) {
        return false;
    }
    
    return true;
}

// 从磁盘读取一个数据块
bool BlockManager::readBlock(uint32_t blockId, char* buffer) {
    if (blockId >= superblock_.totalBlocks) {
        std::cerr << "Block ID out of range: " << blockId << std::endl;
        return false;
    }
    
    if (cache_->get(blockId, buffer)) {
        return true;
    }
    
    if (!disk_->readBlock(blockId, buffer)) {
        return false;
    }
    
    cache_->put(blockId, buffer);
    
    return true;
}

// 将一个数据块写入磁盘
bool BlockManager::writeBlock(uint32_t blockId, const char* buffer) {
    if (blockId >= superblock_.totalBlocks) {
        std::cerr << "Block ID out of range: " << blockId << std::endl;
        return false;
    }
    
    if (!disk_->writeBlock(blockId, buffer)) {
        return false;
    }
    
    cache_->put(blockId, buffer);
    
    return true;
}

// 打印文件系统的统计信息
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

// --- Reference Counting for CoW ---

void BlockManager::incRef(uint32_t blockId) {
    if (blockId == 0 || blockId == INVALID_BLOCK) return;
    refCounts_[blockId]++;
}

void BlockManager::decRef(uint32_t blockId) {
    if (blockId == 0 || blockId == INVALID_BLOCK) return;
    if (refCounts_.find(blockId) != refCounts_.end()) {
        if (refCounts_[blockId] > 0) {
            refCounts_[blockId]--;
        }
        if (refCounts_[blockId] == 0) {
            refCounts_.erase(blockId);
        }
    }
}

uint32_t BlockManager::getRef(uint32_t blockId) const {
    auto it = refCounts_.find(blockId);
    if (it != refCounts_.end()) {
        return it->second;
    }
    // If allocated in bitmap but not in map, assume 1 (legacy/default)
    if (blockId < blockBitmap_.size() && blockBitmap_[blockId]) {
        return 1;
    }
    return 0;
}

bool BlockManager::isShared(uint32_t blockId) const {
    return getRef(blockId) > 1;
}

uint32_t BlockManager::copyOnWrite(uint32_t blockId) {
    if (!isShared(blockId)) {
        return blockId;
    }
    
    // Allocate new block
    uint32_t newBlockId = allocateBlock();
    if (newBlockId == INVALID_BLOCK) {
        return INVALID_BLOCK; // Should handle error
    }
    
    // Copy data
    char buffer[4096];
    if (readBlock(blockId, buffer)) {
        writeBlock(newBlockId, buffer);
    }
    
    // Decrement ref of old block
    decRef(blockId);
    
    return newBlockId;
}

void BlockManager::loadRefCounts() {
    refCounts_.clear();
    std::ifstream infile("refcounts.dat", std::ios::binary);
    if (!infile) return;
    
    size_t size;
    infile.read(reinterpret_cast<char*>(&size), sizeof(size));
    for (size_t i = 0; i < size; ++i) {
        uint32_t blockId;
        uint32_t count;
        infile.read(reinterpret_cast<char*>(&blockId), sizeof(blockId));
        infile.read(reinterpret_cast<char*>(&count), sizeof(count));
        refCounts_[blockId] = count;
    }
}

void BlockManager::saveRefCounts() {
    std::ofstream outfile("refcounts.dat", std::ios::binary);
    if (!outfile) return;
    
    size_t size = refCounts_.size();
    outfile.write(reinterpret_cast<const char*>(&size), sizeof(size));
    for (const auto& pair : refCounts_) {
        outfile.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
        outfile.write(reinterpret_cast<const char*>(&pair.second), sizeof(pair.second));
    }
}

bool BlockManager::clearBlock(uint32_t blockId) {
    char buffer[4096];
    memset(buffer, 0, 4096);
    return writeBlock(blockId, buffer);
}

