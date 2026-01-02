#include "filesystem/disk.h"
#include "filesystem/block_manager.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <cstring>

void testBlockManager() {
    std::cout << "Testing BlockManager..." << std::endl;
    
    // 创建磁盘
    auto disk = std::make_shared<Disk>("test_disk.img");
    assert(disk->create(1024, 4096));
    assert(disk->open());
    
    // 创建 BlockManager，缓存容量为 32
    BlockManager bm(disk, 32);
    
    // 格式化
    assert(bm.format());
    std::cout << "✓ Format successful" << std::endl;
    
    // 分配 inode
    uint32_t inode1 = bm.allocateInode();
    assert(inode1 != INVALID_INODE);
    std::cout << "✓ Allocated inode: " << inode1 << std::endl;
    
    uint32_t inode2 = bm.allocateInode();
    assert(inode2 != INVALID_INODE);
    assert(inode2 != inode1);
    std::cout << "✓ Allocated inode: " << inode2 << std::endl;
    
    // 分配数据块
    uint32_t block1 = bm.allocateBlock();
    assert(block1 != INVALID_BLOCK);
    std::cout << "✓ Allocated block: " << block1 << std::endl;
    
    uint32_t block2 = bm.allocateBlock();
    assert(block2 != INVALID_BLOCK);
    assert(block2 != block1);
    std::cout << "✓ Allocated block: " << block2 << std::endl;
    
    // 写入和读取 inode
    Inode testInode;
    memset(&testInode, 0, sizeof(Inode));
    testInode.type = FileType::REGULAR;
    testInode.size = 1024;
    testInode.blocks = 1;
    testInode.links = 1;
    testInode.uid = 1000;
    testInode.gid = 1000;
    testInode.directBlocks[0] = block1;
    
    assert(bm.writeInode(inode1, testInode));
    std::cout << "✓ Written inode" << std::endl;
    
    Inode readInode;
    assert(bm.readInode(inode1, readInode));
    assert(readInode.type == testInode.type);
    assert(readInode.size == testInode.size);
    assert(readInode.directBlocks[0] == testInode.directBlocks[0]);
    std::cout << "✓ Read inode successfully" << std::endl;
    
    // 写入和读取数据块
    char writeBuffer[4096];
    char readBuffer[4096];
    memset(writeBuffer, 0, 4096);
    memset(readBuffer, 0, 4096);
    strcpy(writeBuffer, "Hello, Filesystem!");
    
    assert(bm.writeBlock(block1, writeBuffer));
    std::cout << "✓ Written block" << std::endl;
    
    assert(bm.readBlock(block1, readBuffer));
    assert(strcmp(readBuffer, writeBuffer) == 0);
    std::cout << "✓ Read block successfully: " << readBuffer << std::endl;
    
    // 再次读取同一块，测试缓存命中
    memset(readBuffer, 0, 4096);
    assert(bm.readBlock(block1, readBuffer));
    std::cout << "✓ Cache hit test: " << readBuffer << std::endl;
    
    // 打印统计信息（包括缓存）
    std::cout << "\n";
    bm.printStats();
    
    // 卸载
    assert(bm.unmount());
    std::cout << "\n✓ Unmounted successfully" << std::endl;
}

void testPersistence() {
    std::cout << "\nTesting Persistence..." << std::endl;
    
    // 重新打开磁盘
    auto disk = std::make_shared<Disk>("test_disk.img");
    assert(disk->open());
    
    // 挂载文件系统
    BlockManager bm(disk, 32);
    assert(bm.mount());
    std::cout << "✓ Mounted successfully" << std::endl;
    
    // 读取之前写入的 inode（会产生缓存未命中）
    // Note: testBlockManager allocated inode 1 (inode 0 is ROOT_INODE)
    Inode readInode;
    assert(bm.readInode(1, readInode));
    assert(readInode.type == FileType::REGULAR);
    assert(readInode.size == 1024);
    std::cout << "✓ Persistence verified" << std::endl;
    
    // 再次读取同一个 inode（会产生缓存命中）
    Inode readInode2;
    assert(bm.readInode(1, readInode2));
    std::cout << "✓ Cache hit on second read" << std::endl;
    
    // 读取数据块
    char buffer[4096];
    assert(bm.readBlock(65, buffer));  // block1 = 65
    std::cout << "✓ Data block read: " << buffer << std::endl;
    
    // 再次读取（缓存命中）
    assert(bm.readBlock(65, buffer));
    std::cout << "✓ Cache hit on data block" << std::endl;
    
    // 打印统计信息
    std::cout << "\n";
    bm.printStats();
    
    bm.unmount();
}

int main() {
    try {
        testBlockManager();
        testPersistence();
        
        std::cout << "\n✅ All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
