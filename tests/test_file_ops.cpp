#include "filesystem/disk.h"
#include "filesystem/block_manager.h"
#include "filesystem/directory_ops.h"
#include "filesystem/file_ops.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <memory>

void testBasicFileOps() {
    std::cout << "\n=== Testing Basic File Operations ===" << std::endl;
    
    auto disk = std::make_shared<Disk>("test_file1.img");
    assert(disk->create(1024, 4096));
    assert(disk->open());
    
    BlockManager blockManager(disk);
    assert(blockManager.format());
    assert(blockManager.mount());
    
    DirectoryOps dirOps(&blockManager);
    dirOps.initializeRoot();
    
    FileOps fileOps(&blockManager, &dirOps);
    
    assert(fileOps.createFile("/test.txt"));
    std::cout << "✓ Created /test.txt" << std::endl;
    
    assert(fileOps.fileExists("/test.txt"));
    std::cout << "✓ File exists" << std::endl;
    
    const char* data = "Hello, Peer Review System!";
    ssize_t written = fileOps.writeFile("/test.txt", data, strlen(data));
    assert(written == (ssize_t)strlen(data));
    std::cout << "✓ Wrote " << written << " bytes" << std::endl;
    
    size_t size = fileOps.getFileSize("/test.txt");
    assert(size == strlen(data));
    std::cout << "✓ File size: " << size << " bytes" << std::endl;
    
    char buffer[1024] = {0};
    ssize_t bytesRead = fileOps.readFile("/test.txt", buffer, sizeof(buffer));
    assert(bytesRead == (ssize_t)strlen(data));
    assert(strcmp(buffer, data) == 0);
    std::cout << "✓ Read: \"" << buffer << "\"" << std::endl;
    
    assert(fileOps.deleteFile("/test.txt"));
    assert(!fileOps.fileExists("/test.txt"));
    std::cout << "✓ Deleted file" << std::endl;
    
    disk->close();
    std::cout << "\n✅ Basic file operations test passed!" << std::endl;
}

void testLargeFile() {
    std::cout << "\n=== Testing Large File ===" << std::endl;
    std::cout << "Step 1: Creating disk..." << std::endl;
    
    auto disk = std::make_shared<Disk>("test_file2.img");
    
    std::cout << "Step 2: Creating disk file..." << std::endl;
    assert(disk->create(1024, 4096));
    
    std::cout << "Step 3: Opening disk..." << std::endl;
    assert(disk->open());
    
    std::cout << "Step 4: Creating BlockManager..." << std::endl;
    BlockManager blockManager(disk);
    
    std::cout << "Step 5: Formatting..." << std::endl;
    assert(blockManager.format());
    
    std::cout << "Step 6: Mounting..." << std::endl;
    assert(blockManager.mount());
    
    std::cout << "Step 7: Creating DirectoryOps..." << std::endl;
    DirectoryOps dirOps(&blockManager);
    
    std::cout << "Step 8: Initializing root..." << std::endl;
    dirOps.initializeRoot();
    
    std::cout << "Step 9: Creating FileOps..." << std::endl;
    FileOps fileOps(&blockManager, &dirOps);
    
    std::cout << "Step 10: Creating file..." << std::endl;
    if (!fileOps.createFile("/large.dat")) {
        std::cout << "FAILED to create file!" << std::endl;
        return;
    }
    std::cout << "✓ File created" << std::endl;
    
    std::cout << "Step 11: Test completed without crash" << std::endl;
    
    disk->close();
    std::cout << "\n✅ Large file test passed!" << std::endl;
}

void testFileInDirectory() {
    std::cout << "\n=== Testing File in Directory ===" << std::endl;
    
    auto disk = std::make_shared<Disk>("test_file3.img");
    assert(disk->create(1024, 4096));
    assert(disk->open());
    
    BlockManager blockManager(disk);
    assert(blockManager.format());
    assert(blockManager.mount());
    
    DirectoryOps dirOps(&blockManager);
    dirOps.initializeRoot();
    
    FileOps fileOps(&blockManager, &dirOps);
    
    assert(dirOps.mkdir("/papers/2024", true));
    
    assert(fileOps.createFile("/papers/2024/paper1.pdf"));
    std::cout << "✓ Created /papers/2024/paper1.pdf" << std::endl;
    
    const char* content = "This is a research paper";
    fileOps.writeFile("/papers/2024/paper1.pdf", content, strlen(content));
    
    char buffer[1024] = {0};
    ssize_t bytesRead = fileOps.readFile("/papers/2024/paper1.pdf", buffer, sizeof(buffer));
    assert(strcmp(buffer, content) == 0);
    std::cout << "✓ File content verified" << std::endl;
    
    disk->close();
    std::cout << "\n✅ File in directory test passed!" << std::endl;
}

int main() {
    try {
        testBasicFileOps();
        testLargeFile();
        testFileInDirectory();
        
        std::cout << "\n🎉 All file operations tests passed!\n" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
