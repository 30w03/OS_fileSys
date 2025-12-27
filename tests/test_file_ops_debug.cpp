#include "filesystem/disk.h"
#include "filesystem/block_manager.h"
#include "filesystem/directory_ops.h"
#include "filesystem/file_ops.h"
#include <iostream>
#include <memory>

int main() {
    std::cout << "1. Creating disk..." << std::endl;
    auto disk = std::make_shared<Disk>("test_debug.img");
    if (!disk->create(1024, 4096)) {
        std::cout << "FAIL: disk->create" << std::endl;
        return 1;
    }
    if (!disk->open()) {
        std::cout << "FAIL: disk->open" << std::endl;
        return 1;
    }
    
    std::cout << "2. Creating BlockManager..." << std::endl;
    BlockManager blockManager(disk);
    if (!blockManager.format()) {
        std::cout << "FAIL: blockManager.format" << std::endl;
        return 1;
    }
    if (!blockManager.mount()) {
        std::cout << "FAIL: blockManager.mount" << std::endl;
        return 1;
    }
    
    std::cout << "3. Initializing DirectoryOps..." << std::endl;
    DirectoryOps dirOps(&blockManager);
    if (!dirOps.initializeRoot()) {
        std::cout << "FAIL: dirOps.initializeRoot" << std::endl;
        return 1;
    }
    
    std::cout << "4. Checking root directory..." << std::endl;
    uint32_t rootInode = dirOps.resolvePath("/");
    std::cout << "   Root inode: " << rootInode << " (should be 0 for ROOT)" << std::endl;
    
    std::cout << "5. Creating FileOps..." << std::endl;
    FileOps fileOps(&blockManager, &dirOps);
    
    uint32_t userId = 0;
    
    std::cout << "6. Calling createFile..." << std::endl;
    bool result = fileOps.createFile(userId, "/test.txt");
    std::cout << "   createFile result: " << (result ? "SUCCESS" : "FAILED") << std::endl;
    
    if (result) {
        std::cout << "7. Verifying file exists..." << std::endl;
        bool exists = fileOps.fileExists(userId, "/test.txt");
        std::cout << "   File exists: " << (exists ? "YES" : "NO") << std::endl;
        
        if (exists) {
            uint32_t fileInode = dirOps.resolvePath("/test.txt");
            std::cout << "   File inode: " << fileInode << std::endl;
        }
    }
    
    disk->close();
    std::cout << "\nDebug test completed!" << std::endl;
    return 0;
}
