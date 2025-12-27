#pragma once

#include <string>
#include <vector>
#include <cstdint>

class Storage {
public:
    Storage(const std::string& diskImage, size_t size);
    ~Storage();
    
    bool mount();
    void unmount();
    bool format();
    
    bool readBlock(uint32_t blockId, std::vector<char>& data);
    bool writeBlock(uint32_t blockId, const std::vector<char>& data);
    
    uint32_t allocateBlock();
    void freeBlock(uint32_t blockId);
    bool isBlockUsed(uint32_t blockId) const;
    
    size_t getBlockSize() const;
    size_t getTotalBlocks() const;
    size_t getFreeBlocks() const;
    
    void printStats() const;  // 添加这行
    
private:
    static constexpr size_t BLOCK_SIZE = 4096;
    
    std::string diskImage_;
    size_t totalBlocks_;
    std::vector<uint8_t> blockBitmap_;
    bool mounted_;
};
