#include "filesystem/directory_ops.h"
#include "filesystem/inode.h"
#include "filesystem/directory.h"
#include <cstring>
#include <sstream>

DirectoryOps::DirectoryOps(BlockManager* blockManager)
    : blockManager_(blockManager), walManager_(nullptr) {}

bool DirectoryOps::initializeRoot() {
    // 检查根目录是否已经存在
    Inode rootInode;
    if (blockManager_->readInode(ROOT_INODE, rootInode)) {
        if (rootInode.type == FileType::DIRECTORY) {
            return true; // 已经初始化
        }
    }

    // 分配根目录 Inode (通常是 0)
    // 注意：allocateInode 可能会返回 0，这是合法的
    uint32_t inodeId = blockManager_->allocateInode();
    if (inodeId == INVALID_INODE) {
        return false;
    }
    
    // 确保分配到的是 ROOT_INODE
    if (inodeId != ROOT_INODE) {
        // 如果不是 0，说明 0 被占用了或者逻辑错误，这里简单处理：
        // 实际系统中可能需要强制指定分配 0 号 inode
        // 但由于我们是刚格式化完，理论上第一个分配的就是 0
    }

    rootInode.type = FileType::DIRECTORY;
    rootInode.size = 0;
    rootInode.blocks = 1;
    rootInode.links = 2; // . and ..
    rootInode.uid = 0;
    rootInode.gid = 0;
    rootInode.atime = rootInode.mtime = rootInode.ctime = std::time(nullptr);
    
    // 分配数据块
    uint32_t blockId = blockManager_->allocateBlock();
    if (blockId == INVALID_BLOCK) {
        return false;
    }
    
    rootInode.directBlocks[0] = blockId;
    
    for (int i = 1; i < MAX_DIRECT_BLOCKS; i++) {
        rootInode.directBlocks[i] = 0;
    }
    rootInode.indirectBlock = 0;
    rootInode.doubleIndirectBlock = 0;
    rootInode.tripleIndirectBlock = 0;
    
    if (!blockManager_->writeInode(ROOT_INODE, rootInode)) {
        return false;
    }
    
    // 初始化目录块（写入 . 和 ..）
    std::vector<DirectoryEntry> entries;
    entries.emplace_back(ROOT_INODE, ".");
    entries.emplace_back(ROOT_INODE, "..");
    
    return writeDirectoryBlock(blockId, entries);
}

bool DirectoryOps::mkdir(const std::string& path, bool recursive) {
    if (path.empty() || path[0] != '/') {
        return false;
    }
    
    if (path == "/") {
        return true;
    }
    
    std::vector<std::string> components = splitPath(path);
    
    if (!recursive) {
        // 非递归模式：只创建最后一级目录，父目录必须存在
        if (components.size() > 1) {
            std::string parentPath = "/";
            for (size_t i = 0; i < components.size() - 1; i++) {
                parentPath += components[i];
                if (i < components.size() - 2) parentPath += "/";
            }
            if (resolvePath(parentPath) == INVALID_INODE) {
                return false;
            }
        }
        return createSingleDirectory(path);
    }
    
    // 递归模式：逐级创建目录
    std::string currentPath = "";
    for (const auto& component : components) {
        currentPath += "/" + component;
        uint32_t inode = resolvePath(currentPath);
        if (inode == INVALID_INODE) {
            if (!createSingleDirectory(currentPath)) {
                return false;
            }
        }
    }
    
    return true;
}

bool DirectoryOps::createSingleDirectory(const std::string& path) {
    size_t lastSlash = path.find_last_of('/');
    std::string parentPath = (lastSlash == 0) ? "/" : path.substr(0, lastSlash);
    std::string dirName = path.substr(lastSlash + 1);
    
    uint32_t parentInode = resolvePath(parentPath);
    if (parentPath != "/" && parentInode == INVALID_INODE) {
        return false;
    }
    // 特殊处理根目录作为父目录的情况，如果 resolvePath 返回 INVALID_INODE 但路径是 /，则应该是 ROOT_INODE
    // 但 resolvePath 应该处理好 / 返回 ROOT_INODE
    if (parentInode == INVALID_INODE) return false;
    
    if (lookup(parentInode, dirName) != INVALID_INODE) {
        return true;  // 目录已存在
    }
    
    // 分配 Inode
    uint32_t newInodeNum = blockManager_->allocateInode();
    if (newInodeNum == INVALID_INODE) {
        return false;
    }
    
    // WAL: 记录创建目录操作
    if (walManager_) {
        walManager_->log(LogOp::MKDIR, newInodeNum, parentInode, dirName);
    }

    Inode dirInode;
    dirInode.type = FileType::DIRECTORY;
    dirInode.size = 0;
    dirInode.blocks = 1;
    dirInode.links = 2;
    dirInode.uid = 0;
    dirInode.gid = 0;
    dirInode.atime = dirInode.mtime = dirInode.ctime = std::time(nullptr);
    
    // 分配数据块
    uint32_t blockId = blockManager_->allocateBlock();
    if (blockId == INVALID_BLOCK) {
        blockManager_->freeInode(newInodeNum);
        return false;
    }
    
    dirInode.directBlocks[0] = blockId;
    
    for (int i = 1; i < MAX_DIRECT_BLOCKS; i++) {
        dirInode.directBlocks[i] = 0;
    }
    dirInode.indirectBlock = 0;
    dirInode.doubleIndirectBlock = 0;
    dirInode.tripleIndirectBlock = 0;
    
    if (!blockManager_->writeInode(newInodeNum, dirInode)) {
        return false;
    }
    
    // 初始化目录内容 (. 和 ..)
    std::vector<DirectoryEntry> entries;
    entries.emplace_back(newInodeNum, ".");
    entries.emplace_back(parentInode, "..");
    
    if (!writeDirectoryBlock(blockId, entries)) {
        return false;
    }
    
    return addEntry(parentInode, dirName, newInodeNum);
}

bool DirectoryOps::rmdir(const std::string& path) {
    uint32_t inodeId = resolvePath(path);
    if (inodeId == INVALID_INODE) {
        return false;
    }
    
    if (!isDirectoryEmpty(inodeId)) {
        return false;
    }
    
    // 获取父目录路径和目录名
    size_t lastSlash = path.find_last_of('/');
    std::string parentPath = (lastSlash == 0) ? "/" : path.substr(0, lastSlash);
    std::string dirName = path.substr(lastSlash + 1);
    
    uint32_t parentInode = resolvePath(parentPath);
    if (parentInode == INVALID_INODE) {
        return false;
    }
    
    // WAL: 记录删除目录操作
    if (walManager_) {
        walManager_->log(LogOp::RMDIR, inodeId, parentInode, dirName);
    }

    // 从父目录中移除条目
    if (!removeEntry(parentInode, dirName)) {
        return false;
    }
    
    // 释放 Inode 和关联的数据块
    Inode inode;
    if (blockManager_->readInode(inodeId, inode)) {
        for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
            if (inode.directBlocks[i] != 0) {
                blockManager_->freeBlock(inode.directBlocks[i]);
            }
        }
        blockManager_->freeInode(inodeId);
    }
    
    return true;
}

std::vector<DirectoryEntry> DirectoryOps::listDirectory(uint32_t inodeId) {
    std::vector<DirectoryEntry> entries;
    
    Inode inode;
    if (!blockManager_->readInode(inodeId, inode)) {
        return entries;
    }
    
    if (inode.type != FileType::DIRECTORY) {
        return entries;
    }
    
    for (uint32_t i = 0; i < inode.blocks && i < MAX_DIRECT_BLOCKS; i++) {
        if (inode.directBlocks[i] != 0) {
            readDirectoryBlock(inode.directBlocks[i], entries);
        }
    }
    
    return entries;
}

uint32_t DirectoryOps::lookup(uint32_t dirInode, const std::string& name) {
    std::vector<DirectoryEntry> entries = listDirectory(dirInode);
    
    for (const auto& entry : entries) {
        if (entry.getName() == name) {
            return entry.inodeNum;
        }
    }
    
    return INVALID_INODE;
}

bool DirectoryOps::addEntry(uint32_t dirInode, const std::string& name, uint32_t inodeId) {
    Inode inode;
    if (!blockManager_->readInode(dirInode, inode)) {
        return false;
    }
    
    if (inode.type != FileType::DIRECTORY) {
        return false;
    }
    
    // 1. Try to find space in existing blocks
    for (uint32_t i = 0; i < inode.blocks && i < MAX_DIRECT_BLOCKS; i++) {
        uint32_t blockId = inode.directBlocks[i];
        char buffer[4096];
        if (!blockManager_->readBlock(blockId, buffer)) {
            continue;
        }
        
        // Scan for free slot
        for (size_t offset = 0; offset < 4096; offset += sizeof(DirectoryEntry)) {
            DirectoryEntry entry;
            entry.deserialize(buffer + offset);
            
            if (!entry.isValid()) {
                // Found free slot
                
                // Check CoW
                if (blockManager_->isShared(blockId)) {
                    uint32_t newBlock = blockManager_->copyOnWrite(blockId);
                    if (newBlock == INVALID_BLOCK) return false;
                    inode.directBlocks[i] = newBlock;
                    blockId = newBlock;
                    // Re-read buffer from new block
                    if (!blockManager_->readBlock(blockId, buffer)) return false;
                }

                DirectoryEntry newEntry(inodeId, name);
                newEntry.serialize(buffer + offset);
                
                if (blockManager_->writeBlock(blockId, buffer)) {
                    inode.mtime = std::time(nullptr);
                    blockManager_->writeInode(dirInode, inode);
                    return true;
                }
                return false;
            }
        }
    }
    
    // 2. No space in existing blocks, allocate new block
    if (inode.blocks >= MAX_DIRECT_BLOCKS) {
        // TODO: Support indirect blocks for directories
        return false;
    }
    
    uint32_t newBlock = blockManager_->allocateBlock();
    if (newBlock == INVALID_BLOCK) {
        return false;
    }
    
    // Initialize new block with the entry at the beginning
    char buffer[4096];
    std::memset(buffer, 0, 4096);
    
    DirectoryEntry newEntry(inodeId, name);
    newEntry.serialize(buffer);
    
    if (!blockManager_->writeBlock(newBlock, buffer)) {
        blockManager_->freeBlock(newBlock);
        return false;
    }
    
    // Update Inode
    inode.directBlocks[inode.blocks] = newBlock;
    inode.blocks++;
    inode.size += 4096; // Directory size usually reflects block usage or entry count
    inode.mtime = std::time(nullptr);
    
    return blockManager_->writeInode(dirInode, inode);
}

bool DirectoryOps::removeEntry(uint32_t dirInode, const std::string& name) {
    Inode inode;
    if (!blockManager_->readInode(dirInode, inode)) {
        return false;
    }
    
    if (inode.type != FileType::DIRECTORY) {
        return false;
    }
    
    for (uint32_t i = 0; i < inode.blocks && i < MAX_DIRECT_BLOCKS; i++) {
        uint32_t blockId = inode.directBlocks[i];
        char buffer[4096];
        if (!blockManager_->readBlock(blockId, buffer)) {
            continue;
        }
        
        bool found = false;
        for (size_t offset = 0; offset < 4096; offset += sizeof(DirectoryEntry)) {
            DirectoryEntry entry;
            entry.deserialize(buffer + offset);
            
            if (entry.isValid() && entry.getName() == name) {
                // Found it! Mark as invalid (delete)
                
                // CoW check
                if (blockManager_->isShared(blockId)) {
                    uint32_t newBlock = blockManager_->copyOnWrite(blockId);
                    if (newBlock == INVALID_BLOCK) return false;
                    inode.directBlocks[i] = newBlock;
                    blockId = newBlock;
                    // Re-read buffer
                    if (!blockManager_->readBlock(blockId, buffer)) return false;
                }

                // We just zero out the inodeNum to mark it as free
                DirectoryEntry emptyEntry; // Default constructor sets INVALID_INODE
                emptyEntry.serialize(buffer + offset);
                found = true;
                break;
            }
        }
        
        if (found) {
            if (blockManager_->writeBlock(blockId, buffer)) {
                inode.mtime = std::time(nullptr);
                blockManager_->writeInode(dirInode, inode);
                return true;
            }
            return false;
        }
    }
    
    return false;
}

bool DirectoryOps::updateEntry(uint32_t dirInode, const std::string& name, uint32_t newInodeId) {
    Inode inode;
    if (!blockManager_->readInode(dirInode, inode)) {
        return false;
    }
    
    if (inode.type != FileType::DIRECTORY) {
        return false;
    }
    
    for (uint32_t i = 0; i < inode.blocks && i < MAX_DIRECT_BLOCKS; i++) {
        uint32_t blockId = inode.directBlocks[i];
        char buffer[4096];
        if (!blockManager_->readBlock(blockId, buffer)) {
            continue;
        }
        
        bool found = false;
        for (size_t offset = 0; offset < 4096; offset += sizeof(DirectoryEntry)) {
            DirectoryEntry entry;
            entry.deserialize(buffer + offset);
            
            if (entry.isValid() && entry.getName() == name) {
                entry.inodeNum = newInodeId;
                entry.serialize(buffer + offset);
                found = true;
                break;
            }
        }
        
        if (found) {
            // Check CoW
            if (blockManager_->isShared(blockId)) {
                uint32_t newBlock = blockManager_->copyOnWrite(blockId);
                if (newBlock == INVALID_BLOCK) return false;
                inode.directBlocks[i] = newBlock;
                blockId = newBlock;
            }

            if (blockManager_->writeBlock(blockId, buffer)) {
                inode.mtime = std::time(nullptr);
                blockManager_->writeInode(dirInode, inode);
                return true;
            }
            return false;
        }
    }
    
    return false;
}

bool DirectoryOps::isEntryInSharedBlock(uint32_t dirInode, const std::string& name) {
    Inode inode;
    if (!blockManager_->readInode(dirInode, inode)) return false;
    
    for (uint32_t i = 0; i < inode.blocks && i < MAX_DIRECT_BLOCKS; i++) {
        uint32_t blockId = inode.directBlocks[i];
        if (blockId == 0) continue;
        
        // Optimization: if block is not shared, entry is not in shared block
        if (!blockManager_->isShared(blockId)) continue;
        
        char buffer[4096];
        if (!blockManager_->readBlock(blockId, buffer)) continue;
        
        for (size_t offset = 0; offset < 4096; offset += sizeof(DirectoryEntry)) {
            DirectoryEntry entry;
            entry.deserialize(buffer + offset);
            if (entry.isValid() && entry.getName() == name) {
                return true;
            }
        }
    }
    return false;
}

uint32_t DirectoryOps::resolvePath(const std::string& path) {
    if (path == "/") {
        return ROOT_INODE;
    }
    
    std::vector<std::string> components = splitPath(path);
    uint32_t currentInode = ROOT_INODE;
    
    for (const auto& component : components) {
        currentInode = lookup(currentInode, component);
        if (currentInode == INVALID_INODE) {
            return INVALID_INODE;
        }
    }
    
    return currentInode;
}

bool DirectoryOps::isDirectoryEmpty(uint32_t inodeId) {
    std::vector<DirectoryEntry> entries = listDirectory(inodeId);
    return entries.size() <= 2;
}

std::vector<std::string> DirectoryOps::splitPath(const std::string& path) {
    std::vector<std::string> components;
    std::stringstream ss(path);
    std::string component;
    
    while (std::getline(ss, component, '/')) {
        if (!component.empty()) {
            components.push_back(component);
        }
    }
    
    return components;
}

bool DirectoryOps::readDirectoryBlock(uint32_t blockId, std::vector<DirectoryEntry>& entries) {
    char buffer[4096];
    if (!blockManager_->readBlock(blockId, buffer)) {
        return false;
    }
    
    size_t offset = 0;
    while (offset + sizeof(DirectoryEntry) <= 4096) {
        DirectoryEntry entry;
        entry.deserialize(buffer + offset);
        if (!entry.isValid()) {
            break;
        }
        entries.push_back(entry);
        offset += sizeof(DirectoryEntry);
    }
    
    return true;
}

bool DirectoryOps::writeDirectoryBlock(uint32_t blockId, const std::vector<DirectoryEntry>& entries) {
    char buffer[4096];
    std::memset(buffer, 0, sizeof(buffer));
    
    size_t offset = 0;
    for (const auto& entry : entries) {
        if (offset + sizeof(DirectoryEntry) > 4096) {
            break;
        }
        entry.serialize(buffer + offset);
        offset += sizeof(DirectoryEntry);
    }
    
    return blockManager_->writeBlock(blockId, buffer);
}

uint32_t DirectoryOps::copyDirectoryBlock(uint32_t blockId) {
    // 1. Allocate new block
    uint32_t newBlockId = blockManager_->allocateBlock();
    if (newBlockId == INVALID_BLOCK) return INVALID_BLOCK;
    
    // 2. Read old block
    char buffer[4096];
    if (!blockManager_->readBlock(blockId, buffer)) {
        blockManager_->freeBlock(newBlockId);
        return INVALID_BLOCK;
    }
    
    // 3. Iterate and Deep Copy Inodes
    for (size_t offset = 0; offset < 4096; offset += sizeof(DirectoryEntry)) {
        DirectoryEntry entry;
        entry.deserialize(buffer + offset);
        if (entry.isValid()) {
            // Allocate new Inode
            uint32_t newInodeId = blockManager_->allocateInode();
            if (newInodeId != INVALID_INODE) {
                Inode inode;
                if (blockManager_->readInode(entry.inodeNum, inode)) {
                    // Copy Inode
                    blockManager_->writeInode(newInodeId, inode);
                    
                    // IncRef Data Blocks
                    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
                        if (inode.directBlocks[i]) blockManager_->incRef(inode.directBlocks[i]);
                    }
                    if (inode.indirectBlock) blockManager_->incRef(inode.indirectBlock);
                    if (inode.doubleIndirectBlock) blockManager_->incRef(inode.doubleIndirectBlock);
                    if (inode.tripleIndirectBlock) blockManager_->incRef(inode.tripleIndirectBlock);
                    
                    // Update Entry
                    entry.inodeNum = newInodeId;
                    entry.serialize(buffer + offset);
                }
            }
        }
    }
    
    // 4. Write new block
    blockManager_->writeBlock(newBlockId, buffer);
    
    // 5. DecRef old block
    blockManager_->decRef(blockId);
    
    return newBlockId;
}





