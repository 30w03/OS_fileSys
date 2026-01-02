#include "filesystem/snapshot.h"
#include "filesystem/inode.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

SnapshotManager::SnapshotManager(std::shared_ptr<BlockManager> blockManager)
    : blockManager_(blockManager) {
    loadSnapshots();
}

uint32_t SnapshotManager::createSnapshot(uint32_t sourceInodeId, const std::string& name) {
    Inode sourceInode;
    if (!blockManager_->readInode(sourceInodeId, sourceInode)) {
        return INVALID_INODE;
    }

    uint32_t snapshotInodeId = blockManager_->allocateInode();
    if (snapshotInodeId == INVALID_INODE) {
        return INVALID_INODE;
    }

    // Copy Inode data
    Inode snapshotInode = sourceInode;
    // Keep links as is? Or reset?
    // Since it's a snapshot, it's a new reference to the data.
    // But the Inode itself is new.
    
    // Increment refcounts for all blocks pointed to by this inode
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        if (snapshotInode.directBlocks[i] != 0) {
            blockManager_->incRef(snapshotInode.directBlocks[i]);
        }
    }
    if (snapshotInode.indirectBlock != 0) {
        blockManager_->incRef(snapshotInode.indirectBlock);
    }
    if (snapshotInode.doubleIndirectBlock != 0) {
        blockManager_->incRef(snapshotInode.doubleIndirectBlock);
    }
    if (snapshotInode.tripleIndirectBlock != 0) {
        blockManager_->incRef(snapshotInode.tripleIndirectBlock);
    }

    if (!blockManager_->writeInode(snapshotInodeId, snapshotInode)) {
        return INVALID_INODE;
    }

    // Save snapshot info
    SnapshotInfo info;
    info.id = snapshotInodeId;
    info.name = name;
    info.timestamp = std::time(nullptr);
    info.sourceInode = sourceInodeId;
    snapshots_.push_back(info);
    saveSnapshots();

    return snapshotInodeId;
}

std::vector<SnapshotInfo> SnapshotManager::listSnapshots() const {
    return snapshots_;
}

bool SnapshotManager::deleteSnapshot(uint32_t snapshotInodeId) {
    // Find snapshot
    auto it = std::find_if(snapshots_.begin(), snapshots_.end(), 
        [snapshotInodeId](const SnapshotInfo& info) { return info.id == snapshotInodeId; });
        
    if (it == snapshots_.end()) return false;
    
    Inode snapshotInode;
    if (blockManager_->readInode(snapshotInodeId, snapshotInode)) {
        // Decrement refcounts
        for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
            if (snapshotInode.directBlocks[i] != 0) {
                blockManager_->decRef(snapshotInode.directBlocks[i]);
            }
        }
        if (snapshotInode.indirectBlock != 0) {
            blockManager_->decRef(snapshotInode.indirectBlock);
        }
        if (snapshotInode.doubleIndirectBlock != 0) {
            blockManager_->decRef(snapshotInode.doubleIndirectBlock);
        }
        if (snapshotInode.tripleIndirectBlock != 0) {
            blockManager_->decRef(snapshotInode.tripleIndirectBlock);
        }
    }
    
    blockManager_->freeInode(snapshotInodeId);
    snapshots_.erase(it);
    saveSnapshots();
    return true;
}

void SnapshotManager::loadSnapshots() {
    snapshots_.clear();
    std::ifstream infile("snapshots.dat", std::ios::binary);
    if (!infile) return;
    
    size_t size;
    infile.read(reinterpret_cast<char*>(&size), sizeof(size));
    for (size_t i = 0; i < size; ++i) {
        SnapshotInfo info;
        infile.read(reinterpret_cast<char*>(&info.id), sizeof(info.id));
        size_t nameLen;
        infile.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        info.name.resize(nameLen);
        infile.read(&info.name[0], nameLen);
        infile.read(reinterpret_cast<char*>(&info.timestamp), sizeof(info.timestamp));
        infile.read(reinterpret_cast<char*>(&info.sourceInode), sizeof(info.sourceInode));
        snapshots_.push_back(info);
    }
}

void SnapshotManager::saveSnapshots() {
    std::ofstream outfile("snapshots.dat", std::ios::binary);
    if (!outfile) return;
    
    size_t size = snapshots_.size();
    outfile.write(reinterpret_cast<const char*>(&size), sizeof(size));
    for (const auto& info : snapshots_) {
        outfile.write(reinterpret_cast<const char*>(&info.id), sizeof(info.id));
        size_t nameLen = info.name.size();
        outfile.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
        outfile.write(info.name.c_str(), nameLen);
        outfile.write(reinterpret_cast<const char*>(&info.timestamp), sizeof(info.timestamp));
        outfile.write(reinterpret_cast<const char*>(&info.sourceInode), sizeof(info.sourceInode));
    }
}
