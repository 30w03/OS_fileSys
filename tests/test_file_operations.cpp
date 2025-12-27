#include "filesystem/filesystem.h"
#include <iostream>
#include <cassert>
#include <cstring>

void testFileCreation() {
    std::cout << "=== Testing File Creation ===" << std::endl;
    
    Filesystem fs("test_file_fs.img");
    assert(fs.format());
    assert(fs.mount());
    
    // 创建文件
    int32_t inode1 = fs.createFile("/test.txt");
    assert(inode1 >= 0);
    std::cout << "Created file /test.txt with inode " << inode1 << std::endl;
    
    // 验证文件存在
    assert(fs.exists("/test.txt"));
    assert(fs.isFile("/test.txt"));
    assert(!fs.isDirectory("/test.txt"));
    
    // 创建第二个文件
    int32_t inode2 = fs.createFile("/another.txt");
    assert(inode2 >= 0);
    assert(inode2 != inode1);
    std::cout << "Created file /another.txt with inode " << inode2 << std::endl;
    
    fs.unmount();
    std::cout << "File creation test passed!" << std::endl;
}

void testFileReadWrite() {
    std::cout << "\n=== Testing File Read/Write ===" << std::endl;
    
    Filesystem fs("test_file_fs.img");
    assert(fs.mount());
    
    // 打开文件
    auto file = fs.openFile("/test.txt");
    assert(file != nullptr);
    
    // 写入数据
    const char* testData = "Hello, Filesystem!";
    int32_t written = file->write(testData, strlen(testData), 0);
    assert(written == (int32_t)strlen(testData));
    std::cout << "Wrote " << written << " bytes" << std::endl;
    
    // 追加数据
    const char* moreData = " This is appended.";
    int32_t appended = file->append(moreData, strlen(moreData));
    assert(appended == (int32_t)strlen(moreData));
    std::cout << "Appended " << appended << " bytes" << std::endl;
    
    // 验证文件大小
    uint32_t expectedSize = strlen(testData) + strlen(moreData);
    assert(file->getSize() == expectedSize);
    std::cout << "File size: " << file->getSize() << " bytes" << std::endl;
    
    // 读取数据
    char readBuffer[1024];
    memset(readBuffer, 0, sizeof(readBuffer));
    int32_t bytesRead = file->read(readBuffer, expectedSize, 0);
    assert(bytesRead == (int32_t)expectedSize);
    std::cout << "Read " << bytesRead << " bytes: " << readBuffer << std::endl;
    
    // 验证数据正确性
    std::string expected = std::string(testData) + std::string(moreData);
    assert(strncmp(readBuffer, expected.c_str(), expectedSize) == 0);
    
    file->flush();
    fs.unmount();
    std::cout << "File read/write test passed!" << std::endl;
}

void testFileDeletion() {
    std::cout << "\n=== Testing File Deletion ===" << std::endl;
    
    Filesystem fs("test_file_fs.img");
    assert(fs.mount());
    
    // 删除文件
    assert(fs.deleteFile("/another.txt"));
    std::cout << "Deleted file /another.txt" << std::endl;
    
    // 验证文件已删除
    assert(!fs.exists("/another.txt"));
    
    fs.unmount();
    std::cout << "File deletion test passed!" << std::endl;
}

void testPersistence() {
    std::cout << "\n=== Testing Persistence ===" << std::endl;
    
    Filesystem fs("test_file_fs.img");
    assert(fs.mount());
    
    // 验证文件仍然存在
    assert(fs.exists("/test.txt"));
    
    // 读取之前写入的数据
    auto file = fs.openFile("/test.txt");
    assert(file != nullptr);
    
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    int32_t bytesRead = file->read(buffer, 1024, 0);
    std::cout << "Read " << bytesRead << " bytes from persisted file: " << buffer << std::endl;
    
    std::string expected = "Hello, Filesystem! This is appended.";
    assert(strncmp(buffer, expected.c_str(), expected.length()) == 0);
    
    fs.printStats();
    fs.unmount();
    std::cout << "Persistence test passed!" << std::endl;
}

void testDirectoryOperations() {
    std::cout << "\n=== Testing Directory Operations ===" << std::endl;
    
    Filesystem fs("test_dir_fs.img");
    assert(fs.format());
    assert(fs.mount());
    
    // 创建目录
    assert(fs.createDirectory("/docs"));
    std::cout << "Created directory /docs" << std::endl;
    
    assert(fs.exists("/docs"));
    assert(fs.isDirectory("/docs"));
    assert(!fs.isFile("/docs"));
    
    // 在目录中创建文件
    int32_t inode = fs.createFile("/docs/readme.txt");
    assert(inode >= 0);
    std::cout << "Created file /docs/readme.txt" << std::endl;
    
    // 列出目录内容
    auto entries = fs.listDirectory("/docs");
    assert(entries.size() == 1);
    assert(entries[0].getName() == "readme.txt");
    std::cout << "Directory /docs contains: " << entries[0].getName() << std::endl;
    
    // 创建嵌套目录
    assert(fs.createDirectory("/docs/sub"));
    std::cout << "Created directory /docs/sub" << std::endl;
    
    // 再次列出目录
    entries = fs.listDirectory("/docs");
    assert(entries.size() == 2);
    std::cout << "Directory /docs now contains " << entries.size() << " entries" << std::endl;
    
    // 在嵌套目录中创建文件
    int32_t nestedFile = fs.createFile("/docs/sub/data.txt");
    assert(nestedFile >= 0);
    std::cout << "Created file /docs/sub/data.txt" << std::endl;
    
    // 验证路径解析
    assert(fs.exists("/docs/sub/data.txt"));
    assert(fs.isFile("/docs/sub/data.txt"));
    
    fs.unmount();
    std::cout << "Directory operations test passed!" << std::endl;
}

void testRootDirectory() {
    std::cout << "\n=== Testing Root Directory ===" << std::endl;
    
    Filesystem fs("test_root_fs.img");
    assert(fs.format());
    assert(fs.mount());
    
    // 在根目录创建多个文件
    assert(fs.createFile("/file1.txt") >= 0);
    assert(fs.createFile("/file2.txt") >= 0);
    assert(fs.createFile("/file3.txt") >= 0);
    
    // 列出根目录
    auto entries = fs.listDirectory("/");
    assert(entries.size() == 3);
    std::cout << "Root directory contains " << entries.size() << " files:" << std::endl;
    for (const auto& entry : entries) {
        std::cout << "  - " << entry.getName() << " (inode " << entry.inodeNum << ")" << std::endl;
    }
    
    fs.unmount();
    std::cout << "Root directory test passed!" << std::endl;
}

int main() {
    try {
        testFileCreation();
        testFileReadWrite();
        testFileDeletion();
        testPersistence();
        testDirectoryOperations();
        testRootDirectory();
        
        std::cout << "\n✅ All file operation tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}