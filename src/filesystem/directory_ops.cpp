#include "filesystem/directory_ops.h"
#include "filesystem/inode.h"
#include "filesystem/directory.h"
#include <cstring>
#include <sstream>

DirectoryOps::DirectoryOps(BlockManager* blockManager)
    : blockManager_(blockManager) {}

bool DirectoryOps::initializeRoot() {
    Inode rootInode;
    rootInode.type = FileType::DIRECTORY;
    rootInode.size = 0;
    rootInode.blocks = 1;
    rootInode.links = 2;
    rootInode.uid = 0;
    rootInode.gid = 0;
    rootInode.atime = rootInode.mtime = rootInode.ctime = std::time(nullptr);
    
    static uint32_t nextDirBlock = 50;
    rootInode.directBlocks[0] = nextDirBlock++;
    
    for (int i = 1; i < MAX_DIRECT_BLOCKS; i++) {
        rootInode.directBlocks[i] = 0;
    }
    rootInode.indirectBlock = 0;
    rootInode.doubleIndirectBlock = 0;
    rootInode.tripleIndirectBlock = 0;
    
    char buffer[4096];
    std::memset(buffer, 0, sizeof(buffer));
    std::memcpy(buffer, &rootInode, sizeof(Inode));
    
    if (!blockManager_->writeBlock(ROOT_INODE, buffer)) {
        return false;
    }
    
    std::memset(buffer, 0, sizeof(buffer));
    return blockManager_->writeBlock(rootInode.directBlocks[0], buffer);
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
            if (resolvePath(parentPath) == 0) {
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
        if (inode == 0) {
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
    if (parentPath != "/" && parentInode == 0) {
        return false;
    }
    
    if (lookup(parentInode, dirName) != 0) {
        return true;  // 目录已存在
    }
    
    static uint32_t nextInode = 10;
    uint32_t newInodeNum = nextInode++;
    
    Inode dirInode;
    dirInode.type = FileType::DIRECTORY;
    dirInode.size = 0;
    dirInode.blocks = 1;
    dirInode.links = 2;
    dirInode.uid = 0;
    dirInode.gid = 0;
    dirInode.atime = dirInode.mtime = dirInode.ctime = std::time(nullptr);
    
    static uint32_t nextDirBlock = 50;
    dirInode.directBlocks[0] = nextDirBlock++;
    
    for (int i = 1; i < MAX_DIRECT_BLOCKS; i++) {
        dirInode.directBlocks[i] = 0;
    }
    dirInode.indirectBlock = 0;
    dirInode.doubleIndirectBlock = 0;
    dirInode.tripleIndirectBlock = 0;
    
    char buffer[4096];
    std::memset(buffer, 0, sizeof(buffer));
    std::memcpy(buffer, &dirInode, sizeof(Inode));
    
    if (!blockManager_->writeBlock(newInodeNum, buffer)) {
        return false;
    }
    
    std::memset(buffer, 0, sizeof(buffer));
    if (!blockManager_->writeBlock(dirInode.directBlocks[0], buffer)) {
        return false;
    }
    
    return addEntry(parentInode, dirName, newInodeNum);
}

bool DirectoryOps::rmdir(const std::string& path) {
    uint32_t inodeId = resolvePath(path);
    if (inodeId == 0) {
        return false;
    }
    return isDirectoryEmpty(inodeId);
}

std::vector<DirectoryEntry> DirectoryOps::listDirectory(uint32_t inodeId) {
    std::vector<DirectoryEntry> entries;
    
    char buffer[4096];
    if (!blockManager_->readBlock(inodeId, buffer)) {
        return entries;
    }
    
    Inode inode;
    std::memcpy(&inode, buffer, sizeof(Inode));
    
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
    
    return 0;
}

bool DirectoryOps::addEntry(uint32_t dirInode, const std::string& name, uint32_t inodeId) {
    char inodeBuffer[4096];
    if (!blockManager_->readBlock(dirInode, inodeBuffer)) {
        return false;
    }
    
    Inode inode;
    std::memcpy(&inode, inodeBuffer, sizeof(Inode));
    
    if (inode.type != FileType::DIRECTORY) {
        return false;
    }
    
    if (inode.blocks == 0) {
        static uint32_t nextDirBlock = 50;
        inode.directBlocks[0] = nextDirBlock++;
        inode.blocks = 1;
    }
    
    std::vector<DirectoryEntry> entries = listDirectory(dirInode);
    DirectoryEntry newEntry(inodeId, name);
    entries.push_back(newEntry);
    
    bool success = writeDirectoryBlock(inode.directBlocks[0], entries);
    
    if (success) {
        inode.mtime = std::time(nullptr);
        std::memset(inodeBuffer, 0, sizeof(inodeBuffer));
        std::memcpy(inodeBuffer, &inode, sizeof(Inode));
        blockManager_->writeBlock(dirInode, inodeBuffer);
    }
    
    return success;
}

bool DirectoryOps::removeEntry(uint32_t dirInode, const std::string& name) {
    char inodeBuffer[4096];
    if (!blockManager_->readBlock(dirInode, inodeBuffer)) {
        return false;
    }
    
    Inode inode;
    std::memcpy(&inode, inodeBuffer, sizeof(Inode));
    
    if (inode.type != FileType::DIRECTORY) {
        return false;
    }
    
    if (inode.blocks == 0) {
        return false;
    }
    
    std::vector<DirectoryEntry> entries = listDirectory(dirInode);
    std::vector<DirectoryEntry> newEntries;
    
    for (const auto& entry : entries) {
        if (entry.getName() != name) {
            newEntries.push_back(entry);
        }
    }
    
    if (newEntries.size() == entries.size()) {
        return false;
    }
    
    bool success = writeDirectoryBlock(inode.directBlocks[0], newEntries);
    
    if (success) {
        inode.mtime = std::time(nullptr);
        std::memset(inodeBuffer, 0, sizeof(inodeBuffer));
        std::memcpy(inodeBuffer, &inode, sizeof(Inode));
        blockManager_->writeBlock(dirInode, inodeBuffer);
    }
    
    return success;
}

uint32_t DirectoryOps::resolvePath(const std::string& path) {
    if (path == "/") {
        return ROOT_INODE;
    }
    
    std::vector<std::string> components = splitPath(path);
    uint32_t currentInode = ROOT_INODE;
    
    for (const auto& component : components) {
        currentInode = lookup(currentInode, component);
        if (currentInode == 0) {
            return 0;
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
