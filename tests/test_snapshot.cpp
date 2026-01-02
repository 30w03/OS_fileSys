#include "filesystem/filesystem.h"
#include "filesystem/snapshot.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>

void test_snapshot() {
    std::string diskImage = "test_snapshot.img";
    Filesystem fs(diskImage);
    
    // Format and mount
    if (!fs.format()) {
        std::cerr << "Format failed" << std::endl;
        return;
    }
    if (!fs.mount()) {
        std::cerr << "Mount failed" << std::endl;
        return;
    }
    
    auto dirOps = fs.getDirOps();
    auto fileOps = fs.getFileOps(); // Need to expose fileOps or use fs methods
    auto snapshotManager = fs.getSnapshotManager();
    
    // 1. Create directory /papers
    if (!dirOps->mkdir("/papers", false)) {
        std::cerr << "mkdir /papers failed" << std::endl;
        return;
    }
    
    // 2. Create file /papers/p1.txt with content "v1"
    if (!fs.createFile("/papers/p1.txt")) {
        std::cerr << "createFile failed" << std::endl;
        return;
    }
    std::string content1 = "version1";
    std::vector<char> data1(content1.begin(), content1.end());
    if (!fs.writeFile("/papers/p1.txt", data1)) {
        std::cerr << "writeFile v1 failed" << std::endl;
        return;
    }
    
    // 3. Create snapshot of /papers
    uint32_t papersInode = dirOps->resolvePath("/papers");
    uint32_t snapInode = snapshotManager->createSnapshot(papersInode, "snap1");
    if (snapInode == INVALID_INODE) {
        std::cerr << "createSnapshot failed" << std::endl;
        return;
    }
    std::cout << "Snapshot created with Inode: " << snapInode << std::endl;
    
    // 4. Modify /papers/p1.txt to "v2"
    std::string content2 = "version2";
    std::vector<char> data2(content2.begin(), content2.end());
    if (!fs.writeFile("/papers/p1.txt", data2)) {
        std::cerr << "writeFile v2 failed" << std::endl;
        return;
    }
    
    // 5. Verify current file content
    std::vector<char> readData;
    if (!fs.readFile("/papers/p1.txt", readData)) {
        std::cerr << "readFile failed" << std::endl;
        return;
    }
    std::string readStr(readData.begin(), readData.end());
    if (readStr != content2) {
        std::cerr << "Current file content mismatch. Expected: " << content2 << ", Got: " << readStr << std::endl;
        return;
    }
    std::cout << "Current file content verified: " << readStr << std::endl;
    
    // 6. Verify snapshot content
    // To verify snapshot, we need to access the file *through* the snapshot inode.
    // But our path resolution starts from ROOT.
    // We can't easily resolve path inside a snapshot unless we mount it or have a way to traverse from a specific inode.
    // DirectoryOps::lookup takes a parent Inode.
    
    // Snapshot Inode is the Inode of "papers" directory at that time.
    // So we can lookup "p1.txt" in snapInode.
    uint32_t p1SnapInode = dirOps->lookup(snapInode, "p1.txt");
    if (p1SnapInode == INVALID_INODE) {
        std::cerr << "p1.txt not found in snapshot" << std::endl;
        return;
    }
    
    // Now read p1SnapInode.
    // FileOps::readFile takes a path. We need a version that takes Inode or we need to manually read.
    // FileOps doesn't expose read by Inode directly in public interface easily (it takes path).
    // But we can use BlockManager to read the inode and then read blocks.
    // Or we can add a helper to FileOps/Filesystem to read by Inode.
    // Or just use BlockManager here since it's a test.
    
    Inode p1Inode;
    fs.getBlockManager()->readInode(p1SnapInode, p1Inode);
    
    // Read data from p1Inode
    std::string snapContent;
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        if (p1Inode.directBlocks[i] != 0) {
            char buffer[4096];
            fs.getBlockManager()->readBlock(p1Inode.directBlocks[i], buffer);
            snapContent += std::string(buffer); // Assuming null terminated or we know size
        }
    }
    // Trim to size
    snapContent = snapContent.substr(0, p1Inode.size);
    
    if (snapContent != content1) {
        std::cerr << "Snapshot content mismatch. Expected: " << content1 << ", Got: " << snapContent << std::endl;
        return;
    }
    std::cout << "Snapshot content verified: " << snapContent << std::endl;
    
    std::cout << "Test Passed!" << std::endl;
}

int main() {
    test_snapshot();
    return 0;
}
