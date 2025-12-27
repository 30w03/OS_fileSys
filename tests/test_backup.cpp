#include "filesystem/backup.h"
#include "filesystem/disk.h"
#include "filesystem/block_manager.h"
#include <iostream>
#include <cassert>
#include <memory>
#include <thread>
#include <chrono>

void testBackupBasic() {
    std::cout << "=== Testing Basic Backup Functionality ===" << std::endl;
    
    // 创建测试磁盘
    auto disk = std::make_shared<Disk>("backup_test.img");
    assert(disk->create(512, 4096));
    assert(disk->open());
    
    BlockManager bm(disk, 32);
    assert(bm.format());
    
    // 写入一些测试数据
    char buffer[4096];
    strcpy(buffer, "Test data for backup");
    assert(bm.writeBlock(65, buffer));
    bm.unmount();
    disk->close();
    
    // 创建备份管理器
    BackupManager backupMgr("backup_test.img");
    
    // 创建第一个备份
    assert(backupMgr.createBackup("Initial backup"));
    std::cout << "✓ First backup created\n" << std::endl;
    
    // 等待1秒（确保时间戳不同）
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 创建第二个备份
    assert(backupMgr.createBackup("Second backup"));
    std::cout << "✓ Second backup created\n" << std::endl;
    
    // 列出所有备份
    backupMgr.printBackups();
    
    std::cout << "\n✅ Basic backup test passed!" << std::endl;
}

void testBackupRestore() {
    std::cout << "\n=== Testing Backup Restore ===" << std::endl;
    
    auto disk = std::make_shared<Disk>("backup_test.img");
    assert(disk->open());
    
    BlockManager bm(disk, 32);
    assert(bm.mount());
    
    // 修改数据
    char buffer[4096];
    strcpy(buffer, "Modified data after backup");
    assert(bm.writeBlock(65, buffer));
    std::cout << "✓ Data modified: " << buffer << std::endl;
    
    bm.unmount();
    disk->close();
    
    // 获取备份列表
    BackupManager backupMgr("backup_test.img");
    auto backups = backupMgr.listBackups();
    assert(!backups.empty());
    
    std::string firstBackup = backups[0].name;
    std::cout << "✓ Restoring from: " << firstBackup << std::endl;
    
    // 恢复第一个备份
    assert(backupMgr.restoreBackup(firstBackup));
    
    // 验证数据已恢复
    disk = std::make_shared<Disk>("backup_test.img");
    assert(disk->open());
    BlockManager bm2(disk, 32);
    assert(bm2.mount());
    
    char readBuffer[4096];
    assert(bm2.readBlock(65, readBuffer));
    std::cout << "✓ Restored data: " << readBuffer << std::endl;
    
    assert(strcmp(readBuffer, "Test data for backup") == 0);
    
    bm2.unmount();
    
    std::cout << "\n✅ Backup restore test passed!" << std::endl;
}

int main() {
    try {
        testBackupBasic();
        testBackupRestore();
        
        std::cout << "\n🎉 All backup tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
