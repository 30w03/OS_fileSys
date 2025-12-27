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
    if (parentPath != "/" && parentInode == INVALID_INODE) {
        return false;
    }
    if (parentInode == INVALID_INODE) return false;
    
    if (dirOps_->lookup(parentInode, fileName) != INVALID_INODE) {
        return false;
    }
    
    uint32_t newInodeNum = blockManager_->allocateInode();
    if (newInodeNum == INVALID_INODE) {
        return false;
    }
    
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
    
    if (!blockManager_->writeInode(newInodeNum, fileInode)) {
        return false;
    }
    
    return dirOps_->addEntry(parentInode, fileName, newInodeNum);
}

bool FileOps::deleteFile(const std::string& path) {
    size_t lastSlash = path.find_last_of('/');
    std::string parentPath = (lastSlash == 0) ? "/" : path.substr(0, lastSlash);
    std::string fileName = path.substr(lastSlash + 1);
    
    uint32_t parentInode = dirOps_->resolvePath(parentPath);
    if (parentPath != "/" && parentInode == INVALID_INODE) {
        return false;
    }
    if (parentInode == INVALID_INODE) return false;
    
    uint32_t fileInodeNum = dirOps_->lookup(parentInode, fileName);
    if (fileInodeNum == INVALID_INODE) {
        return false;
    }
    
    // 从父目录移除条目
    if (!dirOps_->removeEntry(parentInode, fileName)) {
        return false;
    }
    
    // 释放文件占用的资源
    Inode inode;
    if (blockManager_->readInode(fileInodeNum, inode)) {
        for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
            if (inode.directBlocks[i] != 0) {
                blockManager_->freeBlock(inode.directBlocks[i]);
            }
        }
        blockManager_->freeInode(fileInodeNum);
    }
    
    return true;
}

bool FileOps::fileExists(const std::string& path) {
    if (path == "/") {
        return true;
    }
    uint32_t inode = dirOps_->resolvePath(path);
    return inode != INVALID_INODE;
}

ssize_t FileOps::readFile(const std::string& path, char* buffer, size_t size, off_t offset) {
    uint32_t inodeNum = dirOps_->resolvePath(path);
    if (path != "/" && inodeNum == INVALID_INODE) {
        return -1;
    }
    if (inodeNum == INVALID_INODE) return -1;
    
    Inode inode;
    if (!blockManager_->readInode(inodeNum, inode)) {
        return -1;
    }
    
    if (inode.type != FileType::REGULAR) {
        return -1;
    }
    
    if (offset >= inode.size) {
        return 0;
    }
    
    size_t bytesToRead = std::min(size, static_cast<size_t>(inode.size - offset));
    size_t bytesRead = 0;
    
    while (bytesRead < bytesToRead) {
        uint32_t logicalBlock = (offset + bytesRead) / 4096;
        uint32_t blockOffset = (offset + bytesRead) % 4096;
        uint32_t toRead = std::min(bytesToRead - bytesRead, static_cast<size_t>(4096 - blockOffset));
        
        uint32_t physicalBlock = getBlockNumber(inode, logicalBlock);
        
        if (physicalBlock == INVALID_BLOCK || physicalBlock == 0) {
            // Sparse file hole, fill with zeros
            std::memset(buffer + bytesRead, 0, toRead);
        } else {
            char blockBuffer[4096];
            if (!blockManager_->readBlock(physicalBlock, blockBuffer)) {
                return -1;
            }
            std::memcpy(buffer + bytesRead, blockBuffer + blockOffset, toRead);
        }
        
        bytesRead += toRead;
    }
    
    return bytesRead;
}

ssize_t FileOps::writeFile(const std::string& path, const char* data, size_t size, off_t offset) {
    uint32_t inodeNum = dirOps_->resolvePath(path);
    if (path != "/" && inodeNum == INVALID_INODE) {
        return -1;
    }
    if (inodeNum == INVALID_INODE) return -1;
    
    Inode inode;
    if (!blockManager_->readInode(inodeNum, inode)) {
        return -1;
    }
    
    size_t bytesWritten = 0;
    
    while (bytesWritten < size) {
        uint32_t logicalBlock = (offset + bytesWritten) / 4096;
        uint32_t blockOffset = (offset + bytesWritten) % 4096;
        uint32_t toWrite = std::min(size - bytesWritten, static_cast<size_t>(4096 - blockOffset));
        
        uint32_t physicalBlock = getBlockNumber(inode, logicalBlock);
        
        if (physicalBlock == 0 || physicalBlock == INVALID_BLOCK) {
            physicalBlock = blockManager_->allocateBlock();
            if (physicalBlock == INVALID_BLOCK) {
                break; // Disk full
            }
            if (!setBlockNumber(inode, logicalBlock, physicalBlock)) {
                blockManager_->freeBlock(physicalBlock);
                break; // Failed to set block (e.g. limit reached)
            }
            inode.blocks++;
        }
        
        char blockBuffer[4096];
        // If partial write to block, read existing content first
        if (toWrite < 4096) {
             if (!blockManager_->readBlock(physicalBlock, blockBuffer)) {
                 std::memset(blockBuffer, 0, 4096);
             }
        }
        
        std::memcpy(blockBuffer + blockOffset, data + bytesWritten, toWrite);
        
        if (!blockManager_->writeBlock(physicalBlock, blockBuffer)) {
            break;
        }
        
        bytesWritten += toWrite;
    }
    
    if (bytesWritten > 0) {
        inode.size = std::max(inode.size, static_cast<uint32_t>(offset + bytesWritten));
        inode.mtime = std::time(nullptr);
        blockManager_->writeInode(inodeNum, inode);
        return bytesWritten;
    }
    
    return -1;
}

size_t FileOps::getFileSize(const std::string& path) {
    uint32_t inodeNum = dirOps_->resolvePath(path);
    if (path != "/" && inodeNum == INVALID_INODE) {
        return 0;
    }
    if (inodeNum == INVALID_INODE) return 0;
    
    Inode inode;
    if (blockManager_->readInode(inodeNum, inode)) {
        return inode.size;
    }
    
    return 0;
}

bool FileOps::truncate(const std::string& path, size_t newSize) {
    return true;
}

bool FileOps::getFileInfo(const std::string& path, Inode& inode) {
    uint32_t inodeNum = dirOps_->resolvePath(path);
    if (path != "/" && inodeNum == INVALID_INODE) {
        return false;
    }
    if (inodeNum == INVALID_INODE) return false;
    
    return blockManager_->readInode(inodeNum, inode);
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
    return INVALID_BLOCK;
}

bool FileOps::setBlockNumber(Inode& inode, size_t logicalBlock, uint32_t physicalBlock) {
    if (logicalBlock < MAX_DIRECT_BLOCKS) {
        inode.directBlocks[logicalBlock] = physicalBlock;
        return true;
    }
    return false;
}
