#include "filesystem/wal.h"
#include "filesystem/file_ops.h"
#include "filesystem/directory_ops.h"
#include "filesystem/inode.h"
#include <cstring>
#include <iostream>

const uint32_t WAL_MAGIC = 0x57414C47; // "WALG"

WALManager::WALManager(std::shared_ptr<BlockManager> bm, uint32_t startBlock, uint32_t sizeBlocks)
    : bm_(bm), startBlock_(startBlock), sizeBlocks_(sizeBlocks), currentOffset_(0), nextTxnId_(1) {}

bool WALManager::init() {
    std::vector<LogEntry> entries = readAllEntries();
    if (!entries.empty()) {
        nextTxnId_ = entries.back().txnId + 1;
        currentOffset_ = entries.size() * sizeof(LogEntry);
    } else {
        currentOffset_ = 0;
        nextTxnId_ = 1;
    }
    return true;
}

bool WALManager::log(LogOp op, uint32_t inodeId, uint32_t parentInodeId, const std::string& name) {
    if (!loggingEnabled_) return true;

    std::cout << "WAL: Logging Op=" << (int)op << " Inode=" << inodeId 
              << " Parent=" << parentInodeId << " Name=" << name << std::endl;

    LogEntry entry;
    std::memset(&entry, 0, sizeof(LogEntry)); // Zero out padding
    entry.magic = WAL_MAGIC;
    entry.txnId = nextTxnId_++;
    entry.op = op;
    entry.inodeId = inodeId;
    entry.parentInodeId = parentInodeId;
    strncpy(entry.name, name.c_str(), sizeof(entry.name) - 1);
    entry.name[sizeof(entry.name) - 1] = '\0';
    entry.timestamp = std::time(nullptr);
    entry.checksum = calculateChecksum(entry);

    writeEntry(entry);
    return true;
}

bool WALManager::checkpoint() {
    char buffer[4096];
    std::memset(buffer, 0, 4096);
    bm_->writeBlock(startBlock_, buffer); 
    currentOffset_ = 0;
    return true;
}

void WALManager::writeEntry(const LogEntry& entry) {
    uint32_t entrySize = sizeof(LogEntry);
    uint32_t totalBytes = sizeBlocks_ * 4096;
    
    if (currentOffset_ + entrySize > totalBytes) {
        currentOffset_ = 0; 
    }

    uint32_t blockIndex = startBlock_ + (currentOffset_ / 4096);
    uint32_t offsetInBlock = currentOffset_ % 4096;

    // std::cout << "WAL: Writing entry to Block " << blockIndex << " Offset " << offsetInBlock << std::endl;

    char buffer[4096];
    if (!bm_->readBlock(blockIndex, buffer)) {
        std::cerr << "WAL: Failed to read block " << blockIndex << " for writing entry" << std::endl;
        return;
    }
    std::memcpy(buffer + offsetInBlock, &entry, entrySize);
    if (!bm_->writeBlock(blockIndex, buffer)) {
        std::cerr << "WAL: Failed to write block " << blockIndex << " for writing entry" << std::endl;
        return;
    }

    currentOffset_ += entrySize;
}

std::vector<LogEntry> WALManager::readAllEntries() {
    std::vector<LogEntry> entries;
    uint32_t offset = 0;
    uint32_t totalBytes = sizeBlocks_ * 4096;
    char buffer[4096];
    uint32_t currentBlock = -1;

    std::cout << "WAL: Reading entries. StartBlock: " << startBlock_ << " SizeBlocks: " << sizeBlocks_ << std::endl;

    while (offset + sizeof(LogEntry) <= totalBytes) {
        uint32_t blockIndex = startBlock_ + (offset / 4096);
        uint32_t offsetInBlock = offset % 4096;

        if (blockIndex != currentBlock) {
            if (!bm_->readBlock(blockIndex, buffer)) {
                std::cerr << "WAL: Failed to read block " << blockIndex << " during recovery scan" << std::endl;
                break;
            }
            currentBlock = blockIndex;
        }

        LogEntry entry;
        std::memcpy(&entry, buffer + offsetInBlock, sizeof(LogEntry));

        if (entry.magic != WAL_MAGIC) {
            break; 
        }
        
        uint32_t calc = calculateChecksum(entry);
        if (calc == entry.checksum) {
            entries.push_back(entry);
        } else {
            std::cout << "WAL: Checksum mismatch at offset " << offset << " Calc: " << calc << " Read: " << entry.checksum << std::endl;
            break; 
        }

        offset += sizeof(LogEntry);
    }
    return entries;
}

uint32_t WALManager::calculateChecksum(const LogEntry& entry) {
    uint32_t sum = 0;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&entry);
    // Calculate up to the checksum field
    size_t checksumOffset = (const uint8_t*)&entry.checksum - ptr;
    for (size_t i = 0; i < checksumOffset; ++i) {
        sum += ptr[i];
    }
    return sum;
}

bool WALManager::recover(std::shared_ptr<FileOps> fileOps, std::shared_ptr<DirectoryOps> dirOps) {
    std::vector<LogEntry> entries = readAllEntries();
    if (entries.empty()) return true;

    std::cout << "WAL: Recovering " << entries.size() << " entries..." << std::endl;
    
    bool wasEnabled = loggingEnabled_;
    loggingEnabled_ = false;

    for (const auto& entry : entries) {
        std::cout << "Replaying Txn " << entry.txnId << ": Op " << (int)entry.op << std::endl;
        
        if (entry.op == LogOp::CREATE_FILE || entry.op == LogOp::MKDIR) {
            std::cout << "WAL: Replaying CREATE/MKDIR for Inode " << entry.inodeId << std::endl;
            bm_->forceAllocateInode(entry.inodeId);
            
            Inode inode;
            if (bm_->readInode(entry.inodeId, inode)) {
                if (inode.type == FileType::UNUSED) {
                    std::cout << "WAL: Initializing Inode " << entry.inodeId << std::endl;
                    inode.type = (entry.op == LogOp::MKDIR) ? FileType::DIRECTORY : FileType::REGULAR;
                    inode.links = 1;
                    inode.size = 0;
                    inode.atime = inode.mtime = inode.ctime = entry.timestamp;
                    bm_->writeInode(entry.inodeId, inode);
                } else {
                    std::cout << "WAL: Inode " << entry.inodeId << " already in use (Type=" << (int)inode.type << ")" << std::endl;
                }
            } else {
                std::cerr << "WAL: Failed to read Inode " << entry.inodeId << std::endl;
            }

            if (dirOps->lookup(entry.parentInodeId, entry.name) == INVALID_INODE) {
                std::cout << "WAL: Adding directory entry '" << entry.name << "' to parent " << entry.parentInodeId << std::endl;
                dirOps->addEntry(entry.parentInodeId, entry.name, entry.inodeId);
            } else {
                std::cout << "WAL: Directory entry '" << entry.name << "' already exists" << std::endl;
            }

        } else if (entry.op == LogOp::DELETE_FILE || entry.op == LogOp::RMDIR) {
            std::cout << "WAL: Replaying DELETE/RMDIR for Inode " << entry.inodeId << std::endl;
            dirOps->removeEntry(entry.parentInodeId, entry.name);
            bm_->freeInode(entry.inodeId);
        }
    }
    
    loggingEnabled_ = wasEnabled;
    checkpoint(); 
    return true;
}
