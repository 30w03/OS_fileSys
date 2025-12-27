#include "filesystem/block.h"
#include <iostream>
#include <cstring>

BlockManager::BlockManager(std::shared_ptr<Disk> disk)
    : disk_(disk), superblock_(), mounted_(false) {
    
    cache_ = std::make_unique<LRUCache>(Config::CACHE_SIZE);
}

BlockManager::~BlockManager() {
    if (mounted_) {
        unmount();
    }
}

bool BlockManager::format() {
    std::cout << "Formatting filesystem..." << std::endl;
    
    if (!disk_->format()) {
        std::cerr << "Failed to format disk" << std::endl;
        return false;
    }
    
    superblock_ = Superblock();
    
    char buffer[Config::BLOCK_SIZE];
    superblock_.serialize(buffer);
    if (!disk_->writeBlock(Config::SUPERBLOCK_BLOCK, buffer)) {
        std::cerr << "Failed to write superblock" << std::endl;
        return false;
    }
    
    inodeBitmap_ = std::make_unique<Bitmap>(Config::TOTAL_INODES);
    dataBitmap_ = std::make_unique<Bitmap>(Config::TOTAL_BLOCKS - Config::DATA_BLOCK_START);
    
    // 初始化 inode 表为全零
    std::memset(buffer, 0, Config::BLOCK_SIZE);
    for (uint32_t i = 0; i < Config::INODE_BLOCKS; i++) {
        if (!disk_->writeBlock(Config::INODE_TABLE_BLOCK + i, buffer)) {
            std::cerr << "Failed to initialize inode table" << std::endl;
            return false;
        }
    }
    
    // 标记根 inode 为已使用
    inodeBitmap_->set(0);
    
    if (!saveBitmaps()) {
        std::cerr << "Failed to save bitmaps" << std::endl;
        return false;
    }
    
    // 创建根目录 inode
    Inode rootInode;
    rootInode.inodeNum = 0;
    rootInode.type = FileType::DIRECTORY;
    rootInode.size = 0;
    rootInode.blockCount = 0;
    rootInode.mode = 0755;
    rootInode.createTime = std::time(nullptr);
    rootInode.modifyTime = rootInode.createTime;
    rootInode.accessTime = rootInode.createTime;
    
    // 初始化直接块
    for (int i = 0; i < Config::DIRECT_BLOCKS; i++) {
        rootInode.directBlocks[i] = 0;
    }
    
    if (!writeInode(0, rootInode)) {
        std::cerr << "Failed to write root inode" << std::endl;
        return false;
    }
    
    // 更新 superblock
    superblock_.freeInodes--;
    superblock_.serialize(buffer);
    if (!disk_->writeBlock(Config::SUPERBLOCK_BLOCK, buffer)) {
        std::cerr << "Failed to update superblock" << std::endl;
        return false;
    }
    
    std::cout << "Filesystem formatted successfully!" << std::endl;
    
    mounted_ = true;
    return true;
}

bool BlockManager::mount() {
    std::cout << "Mounting filesystem..." << std::endl;
    
    char buffer[Config::BLOCK_SIZE];
    if (!disk_->readBlock(Config::SUPERBLOCK_BLOCK, buffer)) {
        std::cerr << "Failed to read superblock" << std::endl;
        return false;
    }
    
    superblock_.deserialize(buffer);
    
    if (!superblock_.isValid()) {
        std::cerr << "Invalid superblock" << std::endl;
        return false;
    }
    
    inodeBitmap_ = std::make_unique<Bitmap>(Config::TOTAL_INODES);
    
    if (!disk_->readBlock(Config::INODE_BITMAP_BLOCK, buffer)) {
        std::cerr << "Failed to read inode bitmap" << std::endl;
        return false;
    }
    inodeBitmap_->deserialize(buffer);
    
    dataBitmap_ = std::make_unique<Bitmap>(Config::TOTAL_BLOCKS - Config::DATA_BLOCK_START);
    
    if (!disk_->readBlock(Config::DATA_BITMAP_BLOCK, buffer)) {
        std::cerr << "Failed to read data bitmap" << std::endl;
        return false;
    }
    dataBitmap_->deserialize(buffer);
    
    superblock_.mountTime = std::time(nullptr);
    superblock_.serialize(buffer);
    disk_->writeBlock(Config::SUPERBLOCK_BLOCK, buffer);
    
    mounted_ = true;
    std::cout << "Filesystem mounted successfully!" << std::endl;
    
    return true;
}

void BlockManager::unmount() {
    if (!mounted_) {
        return;
    }
    
    std::cout << "Unmounting filesystem..." << std::endl;
    
    saveBitmaps();
    
    char buffer[Config::BLOCK_SIZE];
    superblock_.serialize(buffer);
    disk_->writeBlock(Config::SUPERBLOCK_BLOCK, buffer);
    
    cache_->clear();
    mounted_ = false;
    
    std::cout << "Filesystem unmounted" << std::endl;
}

bool BlockManager::readBlock(uint32_t blockNum, char* buffer) {
    if (!mounted_) {
        return false;
    }
    
    if (cache_->get(blockNum, buffer)) {
        return true;
    }
    
    if (!disk_->readBlock(blockNum, buffer)) {
        return false;
    }
    
    cache_->put(blockNum, buffer);
    return true;
}

bool BlockManager::writeBlock(uint32_t blockNum, const char* buffer) {
    if (!mounted_) {
        return false;
    }
    
    cache_->put(blockNum, buffer);
    return disk_->writeBlock(blockNum, buffer);
}

bool BlockManager::readInode(uint32_t inodeNum, Inode& inode) {
    if (!mounted_) {
        return false;
    }
    
    if (inodeNum >= Config::TOTAL_INODES) {
        return false;
    }
    
    uint32_t inodesPerBlock = Config::BLOCK_SIZE / Config::INODE_SIZE;
    uint32_t blockNum = Config::INODE_TABLE_BLOCK + inodeNum / inodesPerBlock;
    uint32_t offset = (inodeNum % inodesPerBlock) * Config::INODE_SIZE;
    
    char buffer[Config::BLOCK_SIZE];
    if (!readBlock(blockNum, buffer)) {
        return false;
    }
    
    inode.deserialize(buffer + offset);
    return true;
}

bool BlockManager::writeInode(uint32_t inodeNum, const Inode& inode) {
    if (inodeNum >= Config::TOTAL_INODES) {
        std::cerr << "writeInode: invalid inode number " << inodeNum << std::endl;
        return false;
    }
    
    uint32_t inodesPerBlock = Config::BLOCK_SIZE / Config::INODE_SIZE;
    uint32_t blockNum = Config::INODE_TABLE_BLOCK + inodeNum / inodesPerBlock;
    uint32_t offset = (inodeNum % inodesPerBlock) * Config::INODE_SIZE;
    
    char buffer[Config::BLOCK_SIZE];
    
    // 先读取整个块（如果文件系统已挂载）
    if (mounted_) {
        if (!readBlock(blockNum, buffer)) {
            // 如果读取失败，可能是新块，清零
            std::memset(buffer, 0, Config::BLOCK_SIZE);
        }
    } else {
        // 如果还未挂载（在 format 过程中），直接从磁盘读
        if (!disk_->readBlock(blockNum, buffer)) {
            std::memset(buffer, 0, Config::BLOCK_SIZE);
        }
    }
    
    // 序列化 inode 到正确位置
    inode.serialize(buffer + offset);
    
    // 写回
    if (mounted_) {
        return writeBlock(blockNum, buffer);
    } else {
        return disk_->writeBlock(blockNum, buffer);
    }
}

int32_t BlockManager::allocateInode() {
    if (!mounted_) {
        return -1;
    }
    
    int32_t inodeNum = inodeBitmap_->findFree();
    if (inodeNum < 0) {
        return -1;
    }
    
    inodeBitmap_->set(inodeNum);
    superblock_.freeInodes--;
    
    return inodeNum;
}

bool BlockManager::freeInode(uint32_t inodeNum) {
    if (!mounted_) {
        return false;
    }
    
    if (!inodeBitmap_->test(inodeNum)) {
        return false;
    }
    
    inodeBitmap_->clear(inodeNum);
    superblock_.freeInodes++;
    
    return true;
}

int32_t BlockManager::allocateBlock() {
    if (!mounted_) {
        return -1;
    }
    
    int32_t relativeBlockNum = dataBitmap_->findFree();
    if (relativeBlockNum < 0) {
        return -1;
    }
    
    dataBitmap_->set(relativeBlockNum);
    superblock_.freeBlocks--;
    
    return Config::DATA_BLOCK_START + relativeBlockNum;
}

bool BlockManager::freeBlock(uint32_t blockNum) {
    if (!mounted_) {
        return false;
    }
    
    if (blockNum < Config::DATA_BLOCK_START) {
        return false;
    }
    
    uint32_t relativeBlockNum = blockNum - Config::DATA_BLOCK_START;
    
    if (!dataBitmap_->test(relativeBlockNum)) {
        return false;
    }
    
    dataBitmap_->clear(relativeBlockNum);
    superblock_.freeBlocks++;
    
    return true;
}

bool BlockManager::saveBitmaps() {
    char buffer[Config::BLOCK_SIZE];
    
    inodeBitmap_->serialize(buffer);
    if (!disk_->writeBlock(Config::INODE_BITMAP_BLOCK, buffer)) {
        return false;
    }
    
    dataBitmap_->serialize(buffer);
    if (!disk_->writeBlock(Config::DATA_BITMAP_BLOCK, buffer)) {
        return false;
    }
    
    return true;
}

void BlockManager::printStats() const {
    std::cout << "\n=== Filesystem Statistics ===" << std::endl;
    std::cout << "Total blocks: " << superblock_.totalBlocks << std::endl;
    std::cout << "Free blocks: " << superblock_.freeBlocks << std::endl;
    std::cout << "Total inodes: " << superblock_.totalInodes << std::endl;
    std::cout << "Free inodes: " << superblock_.freeInodes << std::endl;
    std::cout << "\nCache statistics:" << std::endl;
    cache_->printStats();
    std::cout << "\nDisk I/O:" << std::endl;
    disk_->printStats();
}