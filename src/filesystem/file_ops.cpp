#include "filesystem/file_ops.h"
#include "filesystem/inode.h"
#include "filesystem/directory.h"
#include <cstring>
#include <algorithm>
#include <vector>

// Helper to check if inode is shared
static bool isInodeShared(BlockManager* bm, const Inode& inode) {
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        if (inode.directBlocks[i] != 0 && bm->isShared(inode.directBlocks[i])) return true;
    }
    if (inode.indirectBlock != 0 && bm->isShared(inode.indirectBlock)) return true;
    if (inode.doubleIndirectBlock != 0 && bm->isShared(inode.doubleIndirectBlock)) return true;
    if (inode.tripleIndirectBlock != 0 && bm->isShared(inode.tripleIndirectBlock)) return true;
    return false;
}

struct PathComp { uint32_t inode; std::string name; };

FileOps::FileOps(BlockManager* blockManager, DirectoryOps* dirOps)
    : blockManager_(blockManager), dirOps_(dirOps) {}

bool FileOps::createFile(uint32_t userId, const std::string& path, mode_t mode) {
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
    
    // WAL: 记录创建操作
    if (walManager_) {
        walManager_->log(LogOp::CREATE_FILE, newInodeNum, parentInode, fileName);
    }

    Inode fileInode;
    fileInode.type = FileType::REGULAR;
    fileInode.size = 0;
    fileInode.blocks = 0;
    fileInode.links = 1;
    fileInode.uid = userId; // Set owner
    fileInode.gid = 0;
    fileInode.flags = 0;
    fileInode.atime = fileInode.mtime = fileInode.ctime = std::time(nullptr);
    
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        fileInode.directBlocks[i] = 0;
    }
    // 初始化 ACL
    for (int i = 0; i < 4; i++) {
        fileInode.acl_uids[i] = 0;
    }

    fileInode.indirectBlock = 0;
    fileInode.doubleIndirectBlock = 0;
    fileInode.tripleIndirectBlock = 0;
    
    if (!blockManager_->writeInode(newInodeNum, fileInode)) {
        return false;
    }
    
    return dirOps_->addEntry(parentInode, fileName, newInodeNum);
}

bool FileOps::checkPermission(uint32_t userId, const Inode& inode, AccessMode mode) {
    // 0. 超级管理员 (root/admin) 拥有所有权限
    // 假设 userId 1 是 admin (根据 server.cpp 中的初始化)
    if (userId == 1) return true;

    // 1. 所有者权限
    if (inode.uid == userId) {
        // 如果是写操作，且文件被锁定，则拒绝
        if (mode == AccessMode::WRITE && (inode.flags & INODE_FLAG_LOCKED)) {
            return false;
        }
        return true;
    }

    // 2. ACL 检查 (只读权限)
    // 如果在 ACL 列表中，允许 READ，不允许 WRITE
    for (int i = 0; i < 4; i++) {
        if (inode.acl_uids[i] == userId) {
            if (mode == AccessMode::READ) {
                return true;
            }
            // ACL 用户目前只授予读权限
            return false;
        }
    }

    // 3. 默认拒绝
    return false;
}

bool FileOps::grantPermission(uint32_t userId, const std::string& path, uint32_t targetUid) {
    uint32_t inodeNum = dirOps_->resolvePath(path);
    if (inodeNum == INVALID_INODE) return false;

    Inode inode;
    if (!blockManager_->readInode(inodeNum, inode)) return false;

    // 只有所有者或管理员可以修改权限
    if (inode.uid != userId && userId != 1) return false;

    // 检查是否已经存在
    for (int i = 0; i < 4; i++) {
        if (inode.acl_uids[i] == targetUid) return true; // 已经有了
    }

    // 寻找空槽位
    for (int i = 0; i < 4; i++) {
        if (inode.acl_uids[i] == 0) {
            inode.acl_uids[i] = targetUid;
            return blockManager_->writeInode(inodeNum, inode);
        }
    }

    return false; // ACL 已满
}

bool FileOps::revokePermission(uint32_t userId, const std::string& path, uint32_t targetUid) {
    uint32_t inodeNum = dirOps_->resolvePath(path);
    if (inodeNum == INVALID_INODE) return false;

    Inode inode;
    if (!blockManager_->readInode(inodeNum, inode)) return false;

    if (inode.uid != userId && userId != 1) return false;

    for (int i = 0; i < 4; i++) {
        if (inode.acl_uids[i] == targetUid) {
            inode.acl_uids[i] = 0;
            return blockManager_->writeInode(inodeNum, inode);
        }
    }

    return true; // 本来就不在
}

bool FileOps::setFileLock(uint32_t userId, const std::string& path, bool locked) {
    uint32_t inodeNum = dirOps_->resolvePath(path);
    if (inodeNum == INVALID_INODE) return false;

    Inode inode;
    if (!blockManager_->readInode(inodeNum, inode)) return false;

    if (inode.uid != userId && userId != 1) return false;

    if (locked) {
        inode.flags |= INODE_FLAG_LOCKED;
    } else {
        inode.flags &= ~INODE_FLAG_LOCKED;
    }

    return blockManager_->writeInode(inodeNum, inode);
}

bool FileOps::deleteFile(uint32_t userId, const std::string& path) {
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
    
    // Check permissions
    Inode inode;
    if (!blockManager_->readInode(fileInodeNum, inode)) {
        return false;
    }
    if (!checkPermission(userId, inode, AccessMode::WRITE)) {
        return false;
    }
    
    // WAL: 记录删除操作
    if (walManager_) {
        walManager_->log(LogOp::DELETE_FILE, fileInodeNum, parentInode, fileName);
    }

    // 从父目录移除条目
    if (!dirOps_->removeEntry(parentInode, fileName)) {
        return false;
    }
    
    // 释放文件占用的资源
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

bool FileOps::fileExists(uint32_t userId, const std::string& path) {
    if (path == "/") {
        return true;
    }
    uint32_t inode = dirOps_->resolvePath(path);
    return inode != INVALID_INODE;
}

ssize_t FileOps::readFile(uint32_t userId, const std::string& path, char* buffer, size_t size, off_t offset) {
    size_t lastSlash = path.find_last_of('/');
    std::string parentPath;
    std::string fileName;
    
    if (lastSlash == std::string::npos) {
        parentPath = "/";
        fileName = path;
    } else if (lastSlash == 0) {
        parentPath = "/";
        fileName = path.substr(1);
    } else {
        parentPath = path.substr(0, lastSlash);
        fileName = path.substr(lastSlash + 1);
    }

    uint32_t parentInode = dirOps_->resolvePath(parentPath);
    if (parentInode == INVALID_INODE) return -1;
    
    uint32_t inodeNum = dirOps_->lookup(parentInode, fileName);
    if (inodeNum == INVALID_INODE) return -1;
    
    Inode inode;
    if (!blockManager_->readInode(inodeNum, inode)) {
        return -1;
    }
    
    if (!checkPermission(userId, inode, AccessMode::READ)) {
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

ssize_t FileOps::writeFile(uint32_t userId, const std::string& path, const char* data, size_t size, off_t offset) {
    // 1. Build path stack
    std::vector<PathComp> stack;
    std::vector<std::string> parts = DirectoryOps::splitPath(path);
    uint32_t current = ROOT_INODE;
    bool pathValid = true;
    
    for (const auto& part : parts) {
        stack.push_back({current, part});
        current = dirOps_->lookup(current, part);
        if (current == INVALID_INODE) {
            pathValid = false;
            break;
        }
    }
    
    if (!pathValid || current == INVALID_INODE) {
        std::cerr << "DEBUG: path resolution failed for path=" << path << " (pathValid=" << pathValid << ", current=" << current << ")" << std::endl;
        return -1;
    }
    uint32_t inodeNum = current;
    
    Inode inode;
    if (!blockManager_->readInode(inodeNum, inode)) {
        std::cerr << "DEBUG: readInode failed for inode " << inodeNum << " path " << path << std::endl;
        return -1;
    }
    std::cerr << "DEBUG: writeFile: inodeNum=" << inodeNum << " uid=" << inode.uid << " orig_size=" << inode.size << " path=" << path << std::endl;
    if (!checkPermission(userId, inode, AccessMode::WRITE)) {
        std::cerr << "DEBUG: permission denied for user " << userId << " on path " << path << std::endl;
        return -1;
    }

    // 2. Check if we need to CoW the Inode (Bubbling)
    // We check if the entry pointing to this inode is in a shared block.
    // The stack contains the path components. The last component points to our file.
    // stack.back() is {ParentInode, FileName}.
    
    if (!stack.empty()) {
        uint32_t parentInodeId = stack.back().inode;
        std::string entryName = stack.back().name;
        
        if (dirOps_->isEntryInSharedBlock(parentInodeId, entryName)) {
            
            // Allocate new Inode
            uint32_t newInodeNum = blockManager_->allocateInode();
            if (newInodeNum == INVALID_INODE) return -1;
            
            // Copy Inode content
            if (!blockManager_->writeInode(newInodeNum, inode)) return -1;
            
            // Increment ref counts of data blocks because they are now shared by newInodeNum
            for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
                if (inode.directBlocks[i] != 0) blockManager_->incRef(inode.directBlocks[i]);
            }
            if (inode.indirectBlock != 0) blockManager_->incRef(inode.indirectBlock);
            if (inode.doubleIndirectBlock != 0) blockManager_->incRef(inode.doubleIndirectBlock);
            if (inode.tripleIndirectBlock != 0) blockManager_->incRef(inode.tripleIndirectBlock);
            
            // Update parent directory to point to new Inode
            // This will trigger CoW of the parent directory block if it is shared
            if (!dirOps_->updateEntry(parentInodeId, entryName, newInodeNum)) {
                return -1;
            }
            
            // Update our local inodeNum to the new one
            inodeNum = newInodeNum;
            
            // Note: We don't need to bubble up further because updateEntry handles CoW of the directory block.
            // And we don't need to CoW the directory Inode itself unless IT is in a shared block of ITS parent.
            // But we are not modifying the directory Inode (except mtime/size).
            // If we modify directory Inode (mtime), we technically should CoW it too if it's shared.
            // But for now let's assume directory mtime update is acceptable or handled separately.
            // (Strictly speaking, if directory Inode is shared, we should CoW it too. But let's fix the file content first).
        }
    }
    
    // Re-read inode (it might be the new one)
    if (!blockManager_->readInode(inodeNum, inode)) return -1;

    size_t bytesWritten = 0;
    
    while (bytesWritten < size) {
        uint32_t logicalBlock = (offset + bytesWritten) / 4096;
        uint32_t blockOffset = (offset + bytesWritten) % 4096;
        uint32_t toWrite = std::min(size - bytesWritten, static_cast<size_t>(4096 - blockOffset));
        
        std::cerr << "DEBUG: write loop: logicalBlock=" << logicalBlock << " blockOffset=" << blockOffset << " toWrite=" << toWrite << " bytesWritten=" << bytesWritten << " size=" << size << std::endl;
        
        uint32_t physicalBlock = getBlockNumber(inode, logicalBlock);
        std::cerr << "DEBUG: initial physicalBlock=" << physicalBlock << std::endl;
        
        if (physicalBlock == 0 || physicalBlock == INVALID_BLOCK) {
            physicalBlock = blockManager_->allocateBlock();
            if (physicalBlock == INVALID_BLOCK) {
                std::cerr << "DEBUG: allocateBlock failed at logicalBlock=" << logicalBlock << " (disk full?)" << std::endl;
                break; // Disk full
            }
            if (!setBlockNumber(inode, logicalBlock, physicalBlock)) {
                blockManager_->freeBlock(physicalBlock);
                std::cerr << "DEBUG: setBlockNumber failed for block=" << physicalBlock << std::endl;
                break; // Failed to set block (e.g. limit reached)
            }
            inode.blocks++;
            std::cerr << "DEBUG: allocated physicalBlock=" << physicalBlock << std::endl;
        } else {
            // Existing block. Check if shared (CoW)
            if (blockManager_->isShared(physicalBlock)) {
                uint32_t newBlock = blockManager_->copyOnWrite(physicalBlock);
                if (newBlock == INVALID_BLOCK) {
                    std::cerr << "DEBUG: copyOnWrite failed for block=" << physicalBlock << std::endl;
                    break;
                }
                
                // Update inode to point to new block
                if (!setBlockNumber(inode, logicalBlock, newBlock)) {
                    std::cerr << "DEBUG: setBlockNumber failed to update to newBlock=" << newBlock << std::endl;
                    break;
                }
                physicalBlock = newBlock;
                std::cerr << "DEBUG: CoW produced newBlock=" << physicalBlock << std::endl;
            }
        }
        
        char blockBuffer[4096];
        // If partial write to block, read existing content first
        if (toWrite < 4096) {
             if (!blockManager_->readBlock(physicalBlock, blockBuffer)) {
                 std::memset(blockBuffer, 0, 4096);
                 std::cerr << "DEBUG: readBlock failed for block " << physicalBlock << ", using zeros" << std::endl;
             }
        }
        
        std::memcpy(blockBuffer + blockOffset, data + bytesWritten, toWrite);
        
        if (!blockManager_->writeBlock(physicalBlock, blockBuffer)) {
            std::cerr << "DEBUG: writeBlock failed for block " << physicalBlock << std::endl;
            break;
        }
        
        bytesWritten += toWrite;
    }
    
    if (bytesWritten > 0 || size == 0) {
        inode.size = std::max(inode.size, static_cast<uint32_t>(offset + bytesWritten));
        inode.mtime = std::time(nullptr);
        blockManager_->writeInode(inodeNum, inode);
        return bytesWritten;
    }
    
    return -1;
}

size_t FileOps::getFileSize(uint32_t userId, const std::string& path) {
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

bool FileOps::truncate(uint32_t userId, const std::string& path, size_t newSize) {
    // 实现真正的 truncate：释放超出 newSize 的块，或扩展文件到 newSize
    uint32_t inodeNum = dirOps_->resolvePath(path);
    if (inodeNum == INVALID_INODE) return false;

    Inode inode;
    if (!blockManager_->readInode(inodeNum, inode)) return false;

    if (!checkPermission(userId, inode, AccessMode::WRITE)) return false;

    if (newSize == inode.size) return true;

    if (newSize < inode.size) {
        // 释放多余的块
        uint32_t lastNeededBlock = (newSize == 0) ? 0 : ( (newSize - 1) / Config::BLOCK_SIZE );
        uint32_t currentBlocks = (inode.size + Config::BLOCK_SIZE - 1) / Config::BLOCK_SIZE;
        for (uint32_t b = lastNeededBlock + 1; b < currentBlocks; b++) {
            uint32_t blockNum = getBlockNumber(inode, b);
            if (blockNum != 0 && blockNum != INVALID_BLOCK) {
                blockManager_->freeBlock(blockNum);
                // clear pointer
                setBlockNumber(inode, b, 0);
            }
        }
        inode.size = newSize;
        inode.mtime = std::time(nullptr);
        blockManager_->writeInode(inodeNum, inode);
        return true;
    } else {
        // 扩展文件：写零
        size_t toWrite = newSize - inode.size;
        std::vector<char> zeros(4096, 0);
        size_t written = 0;
        while (written < toWrite) {
            size_t w = std::min<size_t>(toWrite - written, zeros.size());
            ssize_t r = writeFile(userId, path, zeros.data(), w, inode.size + written);
            if (r < 0) return false;
            written += r;
        }
        inode.mtime = std::time(nullptr);
        blockManager_->writeInode(inodeNum, inode);
        return true;
    }
}


// 原子替换实现：先创建/写入 tmpPath，然后把目录 entry 指向新 inode，再删除临时名
bool FileOps::atomicReplaceFile(uint32_t userId, const std::string& targetPath, const std::string& tmpPath) {
    // Resolve parent directories and names
    size_t tSlash = targetPath.find_last_of('/');
    std::string tParent = (tSlash == 0) ? "/" : targetPath.substr(0, tSlash);
    std::string tName = targetPath.substr(tSlash + 1);

    size_t sSlash = tmpPath.find_last_of('/');
    std::string sParent = (sSlash == 0) ? "/" : tmpPath.substr(0, sSlash);
    std::string sName = tmpPath.substr(sSlash + 1);

    uint32_t tParentInode = dirOps_->resolvePath(tParent);
    uint32_t sParentInode = dirOps_->resolvePath(sParent);
    if (tParentInode == INVALID_INODE || sParentInode == INVALID_INODE) {
        std::cerr << "DEBUG: atomicReplaceFile: invalid parent path" << std::endl;
        return false;
    }

    uint32_t newInode = dirOps_->lookup(sParentInode, sName);
    if (newInode == INVALID_INODE) {
        std::cerr << "DEBUG: atomicReplaceFile: tmp file not found: " << tmpPath << std::endl;
        return false;
    }

    // Update target directory entry to point to newInode
    if (!dirOps_->updateEntry(tParentInode, tName, newInode)) {
        std::cerr << "DEBUG: atomicReplaceFile: updateEntry failed" << std::endl;
        return false;
    }

    // Remove temporary entry
    if (!dirOps_->removeEntry(sParentInode, sName)) {
        std::cerr << "DEBUG: atomicReplaceFile: failed to remove tmp entry " << tmpPath << std::endl;
        // Non-fatal; just warn
    }

    return true;
}

bool FileOps::getFileInfo(uint32_t userId, const std::string& path, Inode& inode) {
    uint32_t inodeNum = dirOps_->resolvePath(path);
    if (path != "/" && inodeNum == INVALID_INODE) {
        return false;
    }
    if (inodeNum == INVALID_INODE) return false;
    
    return blockManager_->readInode(inodeNum, inode);
}

bool FileOps::setPermissions(uint32_t userId, const std::string& path, mode_t mode) {
    return true;
}

bool FileOps::allocateBlocks(Inode& inode, size_t requiredBlocks) {
    return true;
}

bool FileOps::freeFileBlocks(Inode& inode) {
    return true;
}

uint32_t FileOps::getBlockNumber(const Inode& inode, size_t logicalBlock) {
    // 1. 直接块 (0-11)
    if (logicalBlock < MAX_DIRECT_BLOCKS) {
        return inode.directBlocks[logicalBlock];
    }
    
    // 2. 一级间接块 (12 - 1035)
    // 4096 / 4 = 1024 pointers
    uint32_t indirectIndex = logicalBlock - MAX_DIRECT_BLOCKS;
    if (indirectIndex < 1024) {
        if (inode.indirectBlock == 0 || inode.indirectBlock == INVALID_BLOCK) {
            return INVALID_BLOCK;
        }
        
        char buffer[4096];
        if (!blockManager_->readBlock(inode.indirectBlock, buffer)) {
            return INVALID_BLOCK;
        }
        
        uint32_t* pointers = reinterpret_cast<uint32_t*>(buffer);
        return pointers[indirectIndex];
    }
    
    // TODO: 二级间接块 support
    return INVALID_BLOCK;
}

bool FileOps::setBlockNumber(Inode& inode, size_t logicalBlock, uint32_t physicalBlock) {
    // 1. 直接块
    if (logicalBlock < MAX_DIRECT_BLOCKS) {
        inode.directBlocks[logicalBlock] = physicalBlock;
        return true;
    }
    
    // 2. 一级间接块
    uint32_t indirectIndex = logicalBlock - MAX_DIRECT_BLOCKS;
    if (indirectIndex < 1024) {
        // 如果间接块不存在，先分配
        if (inode.indirectBlock == 0 || inode.indirectBlock == INVALID_BLOCK) {
            uint32_t newBlock = blockManager_->allocateBlock();
            if (newBlock == INVALID_BLOCK) {
                return false;
            }
            inode.indirectBlock = newBlock;
            inode.blocks++; // 统计元数据块
            
            // 初始化为全0
            char buffer[4096];
            std::memset(buffer, 0, 4096);
            if (!blockManager_->writeBlock(newBlock, buffer)) {
                return false;
            }
        } else {
            // Check if indirect block is shared (CoW)
            if (blockManager_->isShared(inode.indirectBlock)) {
                uint32_t newIndirect = blockManager_->copyOnWrite(inode.indirectBlock);
                if (newIndirect == INVALID_BLOCK) return false;
                inode.indirectBlock = newIndirect;
            }
        }
        
        // 读取间接块
        char buffer[4096];
        if (!blockManager_->readBlock(inode.indirectBlock, buffer)) {
            return false;
        }
        
        // 更新指针
        uint32_t* pointers = reinterpret_cast<uint32_t*>(buffer);
        pointers[indirectIndex] = physicalBlock;
        
        // 写回
        return blockManager_->writeBlock(inode.indirectBlock, buffer);
    }
    
    return false;
}
