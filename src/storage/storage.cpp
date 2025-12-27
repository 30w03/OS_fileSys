#include "storage/storage.h"
#include <fstream>
#include <cstring>
#include <iostream>

// 预留前10个块用于元数据
static constexpr uint32_t RESERVED_BLOCKS = 10;

Storage::Storage(const std::string& diskImage, size_t size)
    : diskImage_(diskImage), totalBlocks_(size / BLOCK_SIZE), mounted_(false) {
    blockBitmap_.resize((totalBlocks_ + 7) / 8, 0);
    std::cout << "Storage created: " << diskImage_ << " (" << totalBlocks_ << " blocks)" << std::endl;
}

Storage::~Storage() {
    if (mounted_) {
        unmount();
    }
}

bool Storage::mount() {
    if (mounted_) {
        return true;
    }
    
    // Check if disk image exists
    std::ifstream testFile(diskImage_, std::ios::binary);
    bool exists = testFile.good();
    testFile.close();
    
    if (!exists) {
        std::cout << "Disk image not found, creating new one..." << std::endl;
        if (!format()) {
            std::cerr << "Failed to format disk" << std::endl;
            return false;
        }
    }
    
    // Load block bitmap
    std::ifstream file(diskImage_, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open disk image" << std::endl;
        return false;
    }
    
    file.read(reinterpret_cast<char*>(blockBitmap_.data()), blockBitmap_.size());
    file.close();
    
    mounted_ = true;
    
    std::cout << "Storage mounted successfully" << std::endl;
    std::cout << "  Total blocks: " << totalBlocks_ << std::endl;
    std::cout << "  Reserved blocks: " << RESERVED_BLOCKS << std::endl;
    std::cout << "  Free blocks: " << getFreeBlocks() << std::endl;
    std::cout << "  Block size: " << BLOCK_SIZE << " bytes" << std::endl;
    
    return true;
}

void Storage::unmount() {
    if (!mounted_) {
        return;
    }
    
    // Save block bitmap
    std::fstream file(diskImage_, std::ios::binary | std::ios::in | std::ios::out);
    if (file) {
        file.seekp(0);
        file.write(reinterpret_cast<const char*>(blockBitmap_.data()), blockBitmap_.size());
        file.close();
        std::cout << "Storage unmounted, bitmap saved" << std::endl;
    } else {
        std::cerr << "Failed to save bitmap on unmount" << std::endl;
    }
    
    mounted_ = false;
}

bool Storage::format() {
    std::cout << "Formatting disk image..." << std::endl;
    
    // Create disk image file
    std::ofstream file(diskImage_, std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "Failed to create disk image file" << std::endl;
        return false;
    }
    
    // Initialize bitmap: mark reserved blocks as used
    std::fill(blockBitmap_.begin(), blockBitmap_.end(), 0);
    for (uint32_t i = 0; i < RESERVED_BLOCKS && i < totalBlocks_; i++) {
        size_t byteIndex = i / 8;
        size_t bitIndex = i % 8;
        blockBitmap_[byteIndex] |= (1 << bitIndex);
    }
    
    // Write block bitmap
    file.write(reinterpret_cast<const char*>(blockBitmap_.data()), blockBitmap_.size());
    
    // Write empty blocks
    std::vector<char> emptyBlock(BLOCK_SIZE, 0);
    for (size_t i = 0; i < totalBlocks_; i++) {
        file.write(emptyBlock.data(), BLOCK_SIZE);
        
        if (i % 1000 == 0 && i > 0) {
            std::cout << "  Formatted " << i << "/" << totalBlocks_ << " blocks\r" << std::flush;
        }
    }
    
    std::cout << "  Formatted " << totalBlocks_ << "/" << totalBlocks_ << " blocks" << std::endl;
    
    file.close();
    
    if (!file.good()) {
        std::cerr << "Error during format" << std::endl;
        return false;
    }
    
    std::cout << "Format complete (reserved blocks 0-" << (RESERVED_BLOCKS-1) << ")" << std::endl;
    return true;
}

bool Storage::readBlock(uint32_t blockId, std::vector<char>& data) {
    if (!mounted_) {
        std::cerr << "Storage not mounted" << std::endl;
        return false;
    }
    
    if (blockId >= totalBlocks_) {
        std::cerr << "Invalid block ID: " << blockId << " (max: " << totalBlocks_ - 1 << ")" << std::endl;
        return false;
    }
    
    std::ifstream file(diskImage_, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open disk image for reading" << std::endl;
        return false;
    }
    
    // Skip bitmap and seek to block
    size_t offset = blockBitmap_.size() + (static_cast<size_t>(blockId) * BLOCK_SIZE);
    file.seekg(offset);
    
    if (!file.good()) {
        std::cerr << "Failed to seek to block " << blockId << std::endl;
        file.close();
        return false;
    }
    
    data.resize(BLOCK_SIZE);
    file.read(data.data(), BLOCK_SIZE);
    
    bool success = file.good() || file.eof();
    file.close();
    
    if (!success) {
        std::cerr << "Failed to read block " << blockId << std::endl;
    }
    
    return success;
}

bool Storage::writeBlock(uint32_t blockId, const std::vector<char>& data) {
    if (!mounted_) {
        std::cerr << "Storage not mounted" << std::endl;
        return false;
    }
    
    if (blockId >= totalBlocks_) {
        std::cerr << "Invalid block ID: " << blockId << " (max: " << totalBlocks_ - 1 << ")" << std::endl;
        return false;
    }
    
    if (blockId < RESERVED_BLOCKS) {
        std::cerr << "Cannot write to reserved block: " << blockId << std::endl;
        return false;
    }
    
    if (data.size() != BLOCK_SIZE) {
        std::cerr << "Invalid block size: " << data.size() << " (expected: " << BLOCK_SIZE << ")" << std::endl;
        return false;
    }
    
    std::fstream file(diskImage_, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) {
        std::cerr << "Failed to open disk image for writing" << std::endl;
        return false;
    }
    
    // Skip bitmap and seek to block
    size_t offset = blockBitmap_.size() + (static_cast<size_t>(blockId) * BLOCK_SIZE);
    file.seekp(offset);
    
    if (!file.good()) {
        std::cerr << "Failed to seek to block " << blockId << " (offset: " << offset << ")" << std::endl;
        file.close();
        return false;
    }
    
    file.write(data.data(), BLOCK_SIZE);
    file.flush();
    
    bool success = file.good();
    file.close();
    
    if (!success) {
        std::cerr << "Failed to write block " << blockId << std::endl;
    } else {
        std::cout << "Wrote block " << blockId << " successfully" << std::endl;
    }
    
    return success;
}

uint32_t Storage::allocateBlock() {
    if (!mounted_) {
        std::cerr << "Storage not mounted, cannot allocate block" << std::endl;
        return 0;
    }
    
    // Find first free block (skip reserved blocks)
    for (size_t i = RESERVED_BLOCKS; i < totalBlocks_; i++) {
        size_t byteIndex = i / 8;
        size_t bitIndex = i % 8;
        
        if ((blockBitmap_[byteIndex] & (1 << bitIndex)) == 0) {
            // Mark block as used
            blockBitmap_[byteIndex] |= (1 << bitIndex);
            
            std::cout << "Allocated block " << i << " (free: " << getFreeBlocks() << ")" << std::endl;
            
            return static_cast<uint32_t>(i);
        }
    }
    
    std::cerr << "No free blocks available!" << std::endl;
    return 0;
}

void Storage::freeBlock(uint32_t blockId) {
    if (!mounted_) {
        std::cerr << "Storage not mounted" << std::endl;
        return;
    }
    
    if (blockId >= totalBlocks_) {
        std::cerr << "Invalid block ID: " << blockId << std::endl;
        return;
    }
    
    if (blockId < RESERVED_BLOCKS) {
        std::cerr << "Cannot free reserved block: " << blockId << std::endl;
        return;
    }
    
    size_t byteIndex = blockId / 8;
    size_t bitIndex = blockId % 8;
    
    // Mark block as free
    blockBitmap_[byteIndex] &= ~(1 << bitIndex);
    
    std::cout << "Freed block " << blockId << std::endl;
}

bool Storage::isBlockUsed(uint32_t blockId) const {
    if (blockId >= totalBlocks_) {
        return false;
    }
    
    size_t byteIndex = blockId / 8;
    size_t bitIndex = blockId % 8;
    
    return (blockBitmap_[byteIndex] & (1 << bitIndex)) != 0;
}

size_t Storage::getBlockSize() const {
    return BLOCK_SIZE;
}

size_t Storage::getTotalBlocks() const {
    return totalBlocks_;
}

size_t Storage::getFreeBlocks() const {
    size_t free = 0;
    for (size_t i = RESERVED_BLOCKS; i < totalBlocks_; i++) {
        if (!isBlockUsed(i)) {
            free++;
        }
    }
    return free;
}

void Storage::printStats() const {
    std::cout << "\n=== Storage Statistics ===" << std::endl;
    std::cout << "Disk image: " << diskImage_ << std::endl;
    std::cout << "Total blocks: " << totalBlocks_ << std::endl;
    std::cout << "Reserved blocks: " << RESERVED_BLOCKS << std::endl;
    std::cout << "Block size: " << BLOCK_SIZE << " bytes" << std::endl;
    std::cout << "Total capacity: " << (totalBlocks_ * BLOCK_SIZE / 1024 / 1024) << " MB" << std::endl;
    std::cout << "Free blocks: " << getFreeBlocks() << std::endl;
    std::cout << "Used blocks: " << (totalBlocks_ - getFreeBlocks() - RESERVED_BLOCKS) << std::endl;
    std::cout << "=========================\n" << std::endl;
}
