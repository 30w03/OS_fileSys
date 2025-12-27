#include "filesystem/filesystem.h"
#include <cstring>
#include <ctime>
#include <iostream>

Filesystem::Filesystem(const std::string& diskImage)
    : mounted_(false) {
    storage_ = std::make_unique<Storage>(diskImage, 1024 * 1024 * 100);
}

Filesystem::~Filesystem() {
    if (mounted_) {
        unmount();
    }
}

bool Filesystem::mount() {
    if (mounted_) {
        return true;
    }
    
    if (!storage_->mount()) {
        std::cerr << "Storage mount failed" << std::endl;
        return false;
    }
    
    if (!loadInodeTable()) {
        inodeTable_.clear();
        pathToInode_.clear();
        std::cout << "Initialized empty inode table" << std::endl;
    }
    
    mounted_ = true;
    return true;
}

void Filesystem::unmount() {
    if (!mounted_) {
        return;
    }
    
    saveInodeTable();
    storage_->unmount();
    mounted_ = false;
}

bool Filesystem::createFile(const std::string& path) {
    if (!mounted_) {
        std::cerr << "Filesystem not mounted" << std::endl;
        return false;
    }
    
    if (pathToInode_.find(path) != pathToInode_.end()) {
        std::cout << "File already exists: " << path << std::endl;
        return true;
    }
    
    uint32_t inodeId = allocateInode();
    if (inodeId == 0) {
        std::cerr << "Failed to allocate inode" << std::endl;
        return false;
    }
    
    Inode inode;
    inode.id = inodeId;
    inode.size = 0;
    inode.timestamp = static_cast<uint32_t>(std::time(nullptr));
    inode.isDirectory = false;
    
    inodeTable_[inodeId] = inode;
    pathToInode_[path] = inodeId;
    
    std::cout << "Created file: " << path << " (inode " << inodeId << ")" << std::endl;
    
    saveInodeTable();
    
    return true;
}

bool Filesystem::deleteFile(const std::string& path) {
    if (!mounted_) {
        return false;
    }
    
    uint32_t inodeId = findInode(path);
    if (inodeId == 0) {
        return false;
    }
    
    Inode* inode = getInode(inodeId);
    if (!inode || inode->isDirectory) {
        return false;
    }
    
    // Free all blocks
    for (size_t i = 0; i < MAX_BLOCKS_PER_FILE; i++) {
        if (inode->blockPointers[i] != 0) {
            freeBlock(inode->blockPointers[i]);
        }
    }
    
    freeInode(inodeId);
    pathToInode_.erase(path);
    saveInodeTable();
    
    return true;
}

bool Filesystem::readFile(const std::string& path, std::vector<char>& data) {
    if (!mounted_) {
        std::cerr << "Filesystem not mounted" << std::endl;
        return false;
    }
    
    uint32_t inodeId = findInode(path);
    if (inodeId == 0) {
        std::cerr << "File not found: " << path << std::endl;
        return false;
    }
    
    Inode* inode = getInode(inodeId);
    if (!inode || inode->isDirectory) {
        std::cerr << "Not a file: " << path << std::endl;
        return false;
    }
    
    data.clear();
    data.reserve(inode->size);
    
    size_t remaining = inode->size;
    
    // 修改：读取所有块，而不是只读 10 个
    for (size_t i = 0; i < MAX_BLOCKS_PER_FILE && remaining > 0; i++) {
        if (inode->blockPointers[i] == 0) {
            break;
        }
        
        std::vector<char> blockData;
        if (!storage_->readBlock(inode->blockPointers[i], blockData)) {
            std::cerr << "Failed to read block " << inode->blockPointers[i] << std::endl;
            return false;
        }
        
        size_t toCopy = std::min(remaining, blockData.size());
        data.insert(data.end(), blockData.begin(), blockData.begin() + toCopy);
        remaining -= toCopy;
        
        std::cout << "Read block " << i << " (blockId " << inode->blockPointers[i] 
                  << ", " << toCopy << " bytes, total: " << data.size() << "/" << inode->size << ")" << std::endl;
    }
    
    std::cout << "Successfully read file: " << path << " (" << data.size() << " bytes)" << std::endl;
    
    return true;
}

bool Filesystem::writeFile(const std::string& path, const std::vector<char>& data) {
    if (!mounted_) {
        std::cerr << "Filesystem not mounted" << std::endl;
        return false;
    }
    
    std::cout << "writeFile called: " << path << " (" << data.size() << " bytes)" << std::endl;
    
    uint32_t inodeId = findInode(path);
    if (inodeId == 0) {
        std::cout << "Creating new file: " << path << std::endl;
        if (!createFile(path)) {
            std::cerr << "Failed to create file" << std::endl;
            return false;
        }
        inodeId = findInode(path);
        if (inodeId == 0) {
            std::cerr << "Failed to find newly created file" << std::endl;
            return false;
        }
    }
    
    Inode* inode = getInode(inodeId);
    if (!inode) {
        std::cerr << "Failed to get inode" << std::endl;
        return false;
    }
    
    if (inode->isDirectory) {
        std::cerr << "Cannot write to directory" << std::endl;
        return false;
    }
    
    // Free old blocks
    for (size_t i = 0; i < MAX_BLOCKS_PER_FILE; i++) {
        if (inode->blockPointers[i] != 0) {
            freeBlock(inode->blockPointers[i]);
            inode->blockPointers[i] = 0;
        }
    }
    
    // Write data to new blocks
    size_t remaining = data.size();
    size_t offset = 0;
    size_t blockIndex = 0;
    
    while (remaining > 0 && blockIndex < MAX_BLOCKS_PER_FILE) {
        uint32_t blockId = allocateBlock();
        if (blockId == 0) {
            std::cerr << "Failed to allocate block" << std::endl;
            return false;
        }
        
        size_t toWrite = std::min(remaining, storage_->getBlockSize());
        std::vector<char> blockData(storage_->getBlockSize(), 0);
        std::memcpy(blockData.data(), data.data() + offset, toWrite);
        
        if (!storage_->writeBlock(blockId, blockData)) {
            std::cerr << "Failed to write block " << blockId << std::endl;
            freeBlock(blockId);
            return false;
        }
        
        inode->blockPointers[blockIndex++] = blockId;
        offset += toWrite;
        remaining -= toWrite;
        
        std::cout << "Wrote block " << (blockIndex-1) << " (blockId " << blockId 
                  << ", " << toWrite << " bytes)" << std::endl;
    }
    
    if (remaining > 0) {
        std::cerr << "File too large! Maximum size: " 
                  << (MAX_BLOCKS_PER_FILE * storage_->getBlockSize()) << " bytes" << std::endl;
        return false;
    }
    
    inode->size = data.size();
    inode->timestamp = static_cast<uint32_t>(std::time(nullptr));
    
    saveInodeTable();
    
    std::cout << "Successfully wrote file: " << path << " (" << data.size() << " bytes)" << std::endl;
    
    return true;
}

bool Filesystem::isFile(const std::string& path) {
    if (!mounted_) {
        return false;
    }
    
    uint32_t inodeId = findInode(path);
    if (inodeId == 0) {
        return false;
    }
    
    Inode* inode = getInode(inodeId);
    return inode && !inode->isDirectory;
}

bool Filesystem::isDirectory(const std::string& path) {
    if (!mounted_) {
        return false;
    }
    
    uint32_t inodeId = findInode(path);
    if (inodeId == 0) {
        return false;
    }
    
    Inode* inode = getInode(inodeId);
    return inode && inode->isDirectory;
}

std::vector<FileListEntry> Filesystem::listFiles() {
    std::vector<FileListEntry> entries;
    
    if (!mounted_) {
        return entries;
    }
    
    for (const auto& pair : pathToInode_) {
        const std::string& path = pair.first;
        uint32_t inodeId = pair.second;
        
        Inode* inode = getInode(inodeId);
        if (!inode || inode->isDirectory) {
            continue;
        }
        
        FileListEntry entry;
        entry.filename = path;
        entry.size = inode->size;
        entry.timestamp = inode->timestamp;
        
        entries.push_back(entry);
    }
    
    std::cout << "listFiles: returning " << entries.size() << " files" << std::endl;
    
    return entries;
}

std::vector<std::string> Filesystem::listDirectory(const std::string& path) {
    std::vector<std::string> result;
    
    for (const auto& pair : pathToInode_) {
        result.push_back(pair.first);
    }
    
    return result;
}

uint32_t Filesystem::allocateInode() {
    static uint32_t nextId = 1;
    return nextId++;
}

uint32_t Filesystem::allocateBlock() {
    return storage_->allocateBlock();
}

void Filesystem::freeInode(uint32_t id) {
    inodeTable_.erase(id);
}

void Filesystem::freeBlock(uint32_t blockId) {
    storage_->freeBlock(blockId);
}

bool Filesystem::loadInodeTable() {
    inodeTable_.clear();
    pathToInode_.clear();
    return true;
}

bool Filesystem::saveInodeTable() {
    return true;
}

Inode* Filesystem::getInode(uint32_t id) {
    auto it = inodeTable_.find(id);
    if (it == inodeTable_.end()) {
        return nullptr;
    }
    return &it->second;
}

uint32_t Filesystem::findInode(const std::string& path) {
    auto it = pathToInode_.find(path);
    if (it != pathToInode_.end()) {
        return it->second;
    }
    return 0;
}
