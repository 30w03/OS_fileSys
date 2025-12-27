#include "filesystem/disk.h"
#include "filesystem/block_manager.h"
#include "filesystem/directory_ops.h"
#include <iostream>
#include <cassert>
#include <memory>

void testDirectoryBasic() {
    std::cout << "=== Testing Directory Basic Operations ===" << std::endl;
    
    // 创建磁盘
    auto disk = std::make_shared<Disk>("dir_test.img");
    assert(disk->create(1024, 4096));
    assert(disk->open());
    
    BlockManager bm(disk, 32);
    assert(bm.format());
    
    DirectoryOps dirOps(&bm);
    
    // 初始化根目录
    assert(dirOps.initializeRoot());
    std::cout << "✓ Root directory initialized\n" << std::endl;
    
    // 创建目录
    assert(dirOps.mkdir("/papers", false));
    assert(dirOps.mkdir("/users", false));
    std::cout << "✓ Created /papers and /users\n" << std::endl;
    
    // 列出根目录
    auto entries = dirOps.listDirectory(ROOT_INODE);
    std::cout << "Root directory contents:" << std::endl;
    for (const auto& entry : entries) {
        std::cout << "  - " << entry.getName() << " (inode " << entry.inodeNum << ")" << std::endl;
    }
    
    bm.unmount();
    std::cout << "\n✅ Basic directory test passed!" << std::endl;
}

void testRecursiveMkdir() {
    std::cout << "\n=== Testing Recursive Mkdir ===" << std::endl;
    
    auto disk = std::make_shared<Disk>("dir_test.img");
    assert(disk->open());
    
    BlockManager bm(disk, 32);
    assert(bm.mount());
    
    DirectoryOps dirOps(&bm);
    
    // 递归创建多级目录
    assert(dirOps.mkdir("/papers/2024/conference", true));
    std::cout << "✓ Created /papers/2024/conference recursively\n" << std::endl;
    
    // 验证路径
    uint32_t inode = dirOps.resolvePath("/papers/2024/conference");
    assert(inode != INVALID_INODE);
    std::cout << "✓ Path resolved successfully (inode " << inode << ")\n" << std::endl;
    
    // 列出 /papers 目录
    uint32_t papersInode = dirOps.resolvePath("/papers");
    auto entries = dirOps.listDirectory(papersInode);
    std::cout << "/papers directory contents:" << std::endl;
    for (const auto& entry : entries) {
        std::cout << "  - " << entry.getName() << std::endl;
    }
    
    bm.unmount();
    std::cout << "\n✅ Recursive mkdir test passed!" << std::endl;
}

void testRemoveDirectory() {
    std::cout << "\n=== Testing Remove Directory ===" << std::endl;
    
    auto disk = std::make_shared<Disk>("dir_test.img");
    assert(disk->open());
    
    BlockManager bm(disk, 32);
    assert(bm.mount());
    
    DirectoryOps dirOps(&bm);
    
    // 创建测试目录
    assert(dirOps.mkdir("/temp", false));
    std::cout << "✓ Created /temp" << std::endl;
    
    // 删除空目录
    assert(dirOps.rmdir("/temp"));
    std::cout << "✓ Removed /temp" << std::endl;
    
    // 验证已删除
    uint32_t inode = dirOps.resolvePath("/temp");
    assert(inode == INVALID_INODE);
    std::cout << "✓ Verified /temp no longer exists\n" << std::endl;
    
    bm.printStats();
    bm.unmount();
    
    std::cout << "\n✅ Remove directory test passed!" << std::endl;
}

int main() {
    try {
        testDirectoryBasic();
        testRecursiveMkdir();
        testRemoveDirectory();
        
        std::cout << "\n🎉 All directory tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
