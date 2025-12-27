#include "filesystem/disk.h"
#include "filesystem/block_manager.h"
#include "filesystem/directory_ops.h"
#include "filesystem/file_ops.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <memory>
#include <vector>

void testBitmapPersistence() {
    std::cout << "\n=== Testing Bitmap Persistence ===" << std::endl;
    
    std::string diskName = "test_bitmap.img";
    
    // 1. Format and allocate
    {
        auto disk = std::make_shared<Disk>(diskName);
        assert(disk->create(1024, 4096));
        assert(disk->open());
        
        BlockManager bm(disk);
        assert(bm.format());
        assert(bm.mount());
        
        // Allocate inode 1 (0 is root)
        uint32_t inode = bm.allocateInode();
        std::cout << "Allocated inode: " << inode << std::endl;
        assert(inode != INVALID_INODE);
        
        // Allocate block
        uint32_t block = bm.allocateBlock();
        std::cout << "Allocated block: " << block << std::endl;
        assert(block != INVALID_BLOCK);
        
        bm.unmount();
    }
    
    // 2. Remount and verify
    {
        auto disk = std::make_shared<Disk>(diskName);
        assert(disk->open());
        
        BlockManager bm(disk);
        assert(bm.mount());
        
        // Allocate next inode - should NOT be the same as before
        uint32_t nextInode = bm.allocateInode();
        std::cout << "Allocated next inode: " << nextInode << std::endl;
        
        // If persistence works, nextInode should be > previous inode
        // Root(0) + Previous(1) -> Next should be 2
        assert(nextInode > 1); 
        
        bm.unmount();
    }
    
    std::cout << "✅ Bitmap persistence passed!" << std::endl;
}

void testIndirectBlocks() {
    std::cout << "\n=== Testing Indirect Blocks ===" << std::endl;
    
    auto disk = std::make_shared<Disk>("test_indirect.img");
    assert(disk->create(2048, 4096)); // Enough blocks
    assert(disk->open());
    
    BlockManager bm(disk);
    assert(bm.format());
    assert(bm.mount());
    
    DirectoryOps dirOps(&bm);
    dirOps.initializeRoot();
    
    FileOps fileOps(&bm, &dirOps);
    
    uint32_t userId = 0;
    std::string filename = "/large_file.dat";
    assert(fileOps.createFile(userId, filename));
    
    // Write 60KB (15 blocks)
    // Direct blocks: 12 (48KB)
    // Indirect blocks needed: 3
    size_t dataSize = 60 * 1024;
    std::vector<char> data(dataSize);
    for (size_t i = 0; i < dataSize; i++) {
        data[i] = (char)(i % 256);
    }
    
    std::cout << "Writing " << dataSize << " bytes..." << std::endl;
    ssize_t written = fileOps.writeFile(userId, filename, data.data(), dataSize);
    assert(written == (ssize_t)dataSize);
    
    // Verify size
    size_t fileSize = fileOps.getFileSize(userId, filename);
    std::cout << "File size: " << fileSize << std::endl;
    assert(fileSize == dataSize);
    
    // Read back and verify
    std::vector<char> buffer(dataSize);
    ssize_t read = fileOps.readFile(userId, filename, buffer.data(), dataSize);
    assert(read == (ssize_t)dataSize);
    
    if (std::memcmp(data.data(), buffer.data(), dataSize) == 0) {
        std::cout << "Data verification successful!" << std::endl;
    } else {
        std::cerr << "Data verification FAILED!" << std::endl;
        // Find first error
        for(size_t i=0; i<dataSize; i++) {
            if (data[i] != buffer[i]) {
                std::cerr << "Mismatch at offset " << i << ": expected " 
                          << (int)data[i] << ", got " << (int)buffer[i] << std::endl;
                break;
            }
        }
        assert(false);
    }
    
    bm.unmount();
    std::cout << "✅ Indirect blocks passed!" << std::endl;
}

int main() {
    testBitmapPersistence();
    testIndirectBlocks();
    return 0;
}
