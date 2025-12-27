#include "filesystem/file_ops.h"
#include "filesystem/inode.h"
#include "filesystem/directory.h"
#include <cstring>
#include <algorithm>

FileOps::FileOps(BlockManager* blockManager, DirectoryOps* dirOps)
    : blockManager_(blockManager), dirOps_(dirOps) {}

bool FileOps::createFile(const std::string& path, mode_t mode) {
    size_t lastSlash = path.find_last_of('/');
    std::string parentPath = (lastSlash == 0) ? "/" : path.substr(0, lastSlash);
    std::string fileName = path.substr(lastSlash + 1);
    
    uint32_t parentInode = dirOps_->resolvePath(parentPath);
    if (parentPath != "/" && parentInode == 0) {
        return false;
    }
    
    if (dirOps_->lookup(parentInode, fileName) != 0) {
        return false;
    }
    
    static uint32_t nextInode = 1;
    uint32_t newInodeNum = nextInode++;
    
    Inode fileInode;
    fileInode.type = FileType::REGULAR;
    fileInode.size = 0;
    fileInode.blocks = 0;
    fileInode.links = 1;
    fileInode.uid = 0;
    fileInode.gid = 0;
    fileInode.atime = fileInode.mtime = fileInode.ctime = std::time(nullptr);
    
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        fileInode.directBlocks[i] = 0;
    }
    fileInode.indirectBlock = 0;
    fileInode.doubleIndirectBlock = 0;
    fileInode.tripleIndirectBlock = 0;
    
    char buffer[4096];
    std::memset(buffer, 0, sizeof(buffer));
    std::memcpy(buffer, &fileInode, sizeof(Inode));
    
    if (!blockManager_->writeBlock(newInodeNum, buffer)) {
        return false;
    }
    
    return dirOps_->addEntry(parentInode, fileName, newInodeNum);
}

bool FileOps::deleteFile(const std::string& path) {
    size_t lastSlash = path.find_last_of('/');
    std::string parentPath = (lastSlash == 0) ? "/" : path.substr(0, lastSlash);
    std::string fileName = path.substr(lastSlash + 1);
    
    uint32_t parentInode = dirOps_->resolvePath(parentPath);
    if (parentPath != "/" && parentInode == 0) {
        return false;
    }
    
    uint32_t fileInode = dirOps_->lookup(parentInode, fileName);
    if (fileInode == 0) {
        return false;
    }
    
    return dirOps_->removeEntry(parentInode, fileName);
}

bool FileOps::fileExists(const std::string& path) {
    if (path == "/") {
        return true;
    }
    uint32_t inode = dirOps_->resolvePath(path);
    return inode != 0;
}

ssize_t FileOps::readFile(const std::string& path, char* buffer, size_t size, off_t offset) {
    uint32_t inodeNum = dirOps_->resolvePath(path);
    if (path != "/" && inodeNum == 0) {
        return -1;
    }
    
    Inode inode;
    char inodeBuffer[4096];
    if (!blockManager_->readBlock(inodeNum, inodeBuffer)) {
        return -1;
    }
    std::memcpy(&inode, inodeBuffer, sizeof(Inode));
    
    if (inode.type != FileType::REGULAR) {
        return -1;
    }
    
    size_t toRead = std::min(size, static_cast<size_t>(inode.size - offset));
    if (toRead == 0) {
        return 0;
    }
    
    if (inode.blocks > 0 && inode.directBlocks[0] != 0) {
        char blockBuffer[4096];
        if (blockManager_->readBlock(inode.directBlocks[0], blockBuffer)) {
            std::memcpy(buffer, blockBuffer + offset, toRead);
            return toRead;
        }
    }
    
    return -1;
}

ssize_t FileOps::writeFile(const std::string& path, const char* data, size_t size, off_t offset) {
    uint32_t inodeNum = dirOps_->resolvePath(path);
    if (path != "/" && inodeNum == 0) {
        return -1;
    }
    
    Inode inode;
    char inodeBuffer[4096];
    if (!blockManager_->readBlock(inodeNum, inodeBuffer)) {
        return -1;
    }
    std::memcpy(&inode, inodeBuffer, sizeof(Inode));
    
    if (inode.blocks == 0) {
        static uint32_t nextDataBlock = 100;
        inode.directBlocks[0] = nextDataBlock++;
        inode.blocks = 1;
    }
    
    char blockBuffer[4096];
    std::memset(blockBuffer, 0, sizeof(blockBuffer));
    
    if (inode.directBlocks[0] != 0) {
        blockManager_->readBlock(inode.directBlocks[0], blockBuffer);
    }
    
    std::memcpy(blockBuffer + offset, data, size);
    
    if (blockManager_->writeBlock(inode.directBlocks[0], blockBuffer)) {
        inode.size = std::max(inode.size, static_cast<uint32_t>(offset + size));
        inode.mtime = std::time(nullptr);
        
        std::memset(inodeBuffer, 0, sizeof(inodeBuffer));
        std::memcpy(inodeBuffer, &inode, sizeof(Inode));
        blockManager_->writeBlock(inodeNum, inodeBuffer);
        
        return size;
    }
    
    return -1;
}

size_t FileOps::getFileSize(const std::string& path) {
    uint32_t inodeNum = dirOps_->resolvePath(path);
    if (path != "/" && inodeNum == 0) {
        return 0;
    }
    
    char buffer[4096];
    if (blockManager_->readBlock(inodeNum, buffer)) {
        Inode inode;
        std::memcpy(&inode, buffer, sizeof(Inode));
        return inode.size;
    }
    
    return 0;
}

bool FileOps::truncate(const std::string& path, size_t newSize) {
    return true;
}

bool FileOps::getFileInfo(const std::string& path, Inode& inode) {
    uint32_t inodeNum = dirOps_->resolvePath(path);
    if (path != "/" && inodeNum == 0) {
        return false;
    }
    
    char buffer[4096];
    if (blockManager_->readBlock(inodeNum, buffer)) {
        std::memcpy(&inode, buffer, sizeof(Inode));
        return true;
    }
    
    return false;
}

bool FileOps::setPermissions(const std::string& path, mode_t mode) {
    return true;
}

bool FileOps::allocateBlocks(Inode& inode, size_t requiredBlocks) {
    return true;
}

bool FileOps::freeFileBlocks(Inode& inode) {
    return true;
}

uint32_t FileOps::getBlockNumber(const Inode& inode, size_t logicalBlock) {
    if (logicalBlock < MAX_DIRECT_BLOCKS) {
        return inode.directBlocks[logicalBlock];
    }
    return 0;
}

bool FileOps::setBlockNumber(Inode& inode, size_t logicalBlock, uint32_t physicalBlock) {
    if (logicalBlock < MAX_DIRECT_BLOCKS) {
        inode.directBlocks[logicalBlock] = physicalBlock;
        return true;
    }
    return false;
}
