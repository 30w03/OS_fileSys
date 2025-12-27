#ifndef FILE_H
#define FILE_H

#include <string>
#include <memory>
#include "filesystem/block.h"
#include "filesystem/inode.h"

class File {
public:
    File(std::shared_ptr<BlockManager> bm, uint32_t inodeNum);
    ~File();
    
    int32_t read(char* buffer, uint32_t size, uint32_t offset);
    int32_t write(const char* buffer, uint32_t size, uint32_t offset);
    int32_t append(const char* buffer, uint32_t size);
    bool truncate(uint32_t newSize);
    uint32_t getSize() const;
    bool getInode(Inode& inode) const;
    bool flush();
    
private:
    std::shared_ptr<BlockManager> blockManager_;
    uint32_t inodeNum_;
    Inode inode_;
    bool dirty_;
    
    uint32_t getBlockNum(uint32_t logicalBlock);
    bool allocateBlock(uint32_t logicalBlock);
    void loadInode();
    void saveInode();
};

#endif