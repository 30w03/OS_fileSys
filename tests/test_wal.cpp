#include "filesystem/filesystem.h"
#include "filesystem/wal.h"
#include "filesystem/inode.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>

void test_wal_recovery() {
    std::cout << "Testing WAL Recovery..." << std::endl;
    std::string diskName = "test_wal.img";
    
    // 1. Prepare: Format the disk
    {
        Filesystem fs(diskName);
        if (!fs.format()) {
            std::cerr << "Format failed" << std::endl;
            exit(1);
        }
    }

    // 2. Simulate "Crash"
    // We will manually write a log entry but NOT do the actual operation.
    // This simulates the state where we wrote the log and then power failed.
    uint32_t targetInode = 10; // Arbitrary inode
    std::string fileName = "recovered_file.txt";
    
    {
        Filesystem fs(diskName);
        fs.mount();
        
        auto wal = fs.getWALManager();
        auto bm = fs.getBlockManager();
        
        std::cout << "Simulating crash: Logging CREATE_FILE but not creating it..." << std::endl;
        // Log the intent to create a file named "recovered_file.txt" at inode 10 under ROOT_INODE
        wal->log(LogOp::CREATE_FILE, targetInode, ROOT_INODE, fileName);
        
        // CRITICAL: We do NOT call fileOps->createFile.
        // We simulate a crash immediately after the log is written.
        // The Filesystem destructor will run, but it only calls unmount(), which flushes block cache.
        // It does NOT clear the WAL (checkpoint is not called in unmount).
    }
    
    // 3. Recover
    {
        std::cout << "Mounting again to trigger recovery..." << std::endl;
        Filesystem fs(diskName);
        // mount() triggers walManager_->recover()
        if (!fs.mount()) {
            std::cerr << "Mount failed" << std::endl;
            exit(1);
        }
        
        // 4. Verify
        if (fs.exists("/" + fileName)) {
            std::cout << "SUCCESS: File '" << fileName << "' was recovered!" << std::endl;
        } else {
            std::cerr << "FAILURE: File '" << fileName << "' was NOT recovered." << std::endl;
            exit(1);
        }
        
        // Verify Inode is allocated
        Inode inode;
        if (fs.getBlockManager()->readInode(targetInode, inode)) {
             if (inode.type == FileType::REGULAR) {
                 std::cout << "SUCCESS: Inode " << targetInode << " type is REGULAR." << std::endl;
             } else {
                 std::cerr << "FAILURE: Inode " << targetInode << " type is incorrect. Expected REGULAR." << std::endl;
                 exit(1);
             }
        } else {
             std::cerr << "FAILURE: Could not read Inode " << targetInode << std::endl;
             exit(1);
        }
    }
}

void test_wal_mkdir_recovery() {
    std::cout << "\nTesting WAL Mkdir Recovery..." << std::endl;
    std::string diskName = "test_wal_mkdir.img";
    
    // 1. Format
    {
        Filesystem fs(diskName);
        fs.format();
    }

    uint32_t targetInode = 20;
    std::string dirName = "recovered_dir";

    // 2. Simulate Crash
    {
        Filesystem fs(diskName);
        fs.mount();
        auto wal = fs.getWALManager();
        
        std::cout << "Simulating crash: Logging MKDIR..." << std::endl;
        wal->log(LogOp::MKDIR, targetInode, ROOT_INODE, dirName);
    }

    // 3. Recover
    {
        Filesystem fs(diskName);
        fs.mount();
        
        if (fs.exists("/" + dirName)) {
             std::cout << "SUCCESS: Directory '" << dirName << "' was recovered!" << std::endl;
        } else {
             std::cerr << "FAILURE: Directory '" << dirName << "' was NOT recovered." << std::endl;
             exit(1);
        }
        
        Inode inode;
        if (fs.getBlockManager()->readInode(targetInode, inode)) {
             if (inode.type == FileType::DIRECTORY) {
                 std::cout << "SUCCESS: Inode " << targetInode << " type is DIRECTORY." << std::endl;
             } else {
                 std::cerr << "FAILURE: Inode " << targetInode << " type is incorrect." << std::endl;
                 exit(1);
             }
        }
    }
}

void test_wal_delete_recovery() {
    std::cout << "\nTesting WAL Delete Recovery..." << std::endl;
    std::string diskName = "test_wal_delete.img";
    std::string fileName = "to_be_deleted.txt";
    uint32_t fileInode = 0;

    // 1. Prepare: Create a file normally
    {
        Filesystem fs(diskName);
        fs.format();
        fs.mount();
        fs.createFile("/" + fileName);
        fileInode = fs.getDirOps()->lookup(ROOT_INODE, fileName);
        assert(fileInode != INVALID_INODE);
        std::cout << "Created file " << fileName << " with inode " << fileInode << std::endl;
    }

    // 2. Simulate Crash during Delete
    // We log DELETE but don't actually delete it from directory or free inode
    {
        Filesystem fs(diskName);
        fs.mount();
        auto wal = fs.getWALManager();
        
        std::cout << "Simulating crash: Logging DELETE_FILE..." << std::endl;
        wal->log(LogOp::DELETE_FILE, fileInode, ROOT_INODE, fileName);
    }

    // 3. Recover
    {
        Filesystem fs(diskName);
        fs.mount();
        
        if (!fs.exists("/" + fileName)) {
             std::cout << "SUCCESS: File '" << fileName << "' was deleted during recovery!" << std::endl;
        } else {
             std::cerr << "FAILURE: File '" << fileName << "' still exists." << std::endl;
             exit(1);
        }
        
        // Verify Inode is freed (or marked unused)
        Inode inode;
        // Note: readInode might succeed even if freed if we don't check bitmap, 
        // but freeInode usually clears the inode content on disk too.
        // Let's check if it's UNUSED.
        fs.getBlockManager()->readInode(fileInode, inode);
        if (inode.type == FileType::UNUSED) {
             std::cout << "SUCCESS: Inode " << fileInode << " is UNUSED." << std::endl;
        } else {
             std::cerr << "FAILURE: Inode " << fileInode << " is NOT UNUSED." << std::endl;
             // It might be that freeInode logic in recovery only updates bitmap?
             // Let's check wal.cpp: 
             // } else if (entry.op == LogOp::DELETE_FILE || entry.op == LogOp::RMDIR) {
             //    dirOps->removeEntry(entry.parentInodeId, entry.name);
             //    bm_->freeInode(entry.inodeId);
             // }
             // bm_->freeInode writes a cleared inode to disk. So it should be UNUSED.
             exit(1);
        }
    }
}

int main() {
    test_wal_recovery();
    test_wal_mkdir_recovery();
    test_wal_delete_recovery();
    
    std::cout << "\nAll WAL tests passed!" << std::endl;
    return 0;
}
