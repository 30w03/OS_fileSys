#ifndef DISK_H
#define DISK_H

#include <string>
#include <fstream>
#include <cstdint>
#include <vector>

class Disk {
public:
    explicit Disk(const std::string& filename);
    ~Disk();
    
    bool create(uint32_t totalBlocks, uint32_t blockSize);
    bool open();
    void close();
    
    bool readBlock(uint32_t blockId, char* buffer);
    bool writeBlock(uint32_t blockId, const char* buffer);
    
    uint32_t getTotalBlocks() const;
    uint32_t getBlockSize() const;
    
private:
    std::string filename_;
    std::fstream file_;
    uint32_t totalBlocks_;
    uint32_t blockSize_;
};

#endif // DISK_H
