#include "filesystem/filesystem.h"
#include "filesystem/file_ops.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>
#include <memory>

// 简单的断言宏，打印更多信息
#define ASSERT_TRUE(cond, msg) \
    if (!(cond)) { \
        std::cerr << "❌ Test Failed: " << msg << " (Line " << __LINE__ << ")" << std::endl; \
        return 1; \
    } else { \
        std::cout << "✅ " << msg << std::endl; \
    }

#define ASSERT_FALSE(cond, msg) \
    if (cond) { \
        std::cerr << "❌ Test Failed: " << msg << " (Line " << __LINE__ << ")" << std::endl; \
        return 1; \
    } else { \
        std::cout << "✅ " << msg << std::endl; \
    }

int main() {
    std::cout << "Starting ACL and Locking Tests..." << std::endl;

    // 1. 初始化文件系统
    // 使用内存中的虚拟磁盘或临时文件
    std::string diskImage = "test_acl.img";
    auto fs = std::make_shared<Filesystem>(diskImage);
    
    // 格式化并挂载
    if (!fs->format(4096, 100)) {
        std::cerr << "Failed to format filesystem" << std::endl;
        return 1;
    }
    if (!fs->mount()) {
        std::cerr << "Failed to mount filesystem" << std::endl;
        return 1;
    }

    auto fileOps = fs->getFileOps();
    
    // 定义用户 ID
    uint32_t ADMIN_ID = 1;
    uint32_t OWNER_ID = 100;
    uint32_t REVIEWER_ID = 200;
    uint32_t STRANGER_ID = 300;
    
    std::string testFile = "/paper.pdf";
    std::string content = "This is a confidential paper.";
    
    // 2. Owner 创建文件
    ASSERT_TRUE(fileOps->createFile(OWNER_ID, testFile), "Owner creates file");
    ASSERT_TRUE(fileOps->writeFile(OWNER_ID, testFile, content.c_str(), content.length()) == (ssize_t)content.length(), "Owner writes file");
    
    // 3. 基础权限测试
    char buffer[100];
    ASSERT_TRUE(fileOps->readFile(OWNER_ID, testFile, buffer, 100) > 0, "Owner reads file");
    ASSERT_FALSE(fileOps->readFile(STRANGER_ID, testFile, buffer, 100) > 0, "Stranger cannot read file");
    ASSERT_FALSE(fileOps->writeFile(STRANGER_ID, testFile, "hack", 4) > 0, "Stranger cannot write file");

    // 4. ACL 测试 (Reviewer)
    std::cout << "\n--- ACL Tests ---" << std::endl;
    // 初始状态：Reviewer 不能读
    ASSERT_FALSE(fileOps->readFile(REVIEWER_ID, testFile, buffer, 100) > 0, "Reviewer cannot read initially");
    
    // 授权
    ASSERT_TRUE(fileOps->grantPermission(OWNER_ID, testFile, REVIEWER_ID), "Owner grants permission to Reviewer");
    
    // 授权后：Reviewer 能读
    memset(buffer, 0, 100);
    ASSERT_TRUE(fileOps->readFile(REVIEWER_ID, testFile, buffer, 100) > 0, "Reviewer can read after grant");
    // 简单的内容检查
    bool contentMatch = true;
    for(size_t i=0; i<content.length(); ++i) {
        if(buffer[i] != content[i]) contentMatch = false;
    }
    ASSERT_TRUE(contentMatch, "Reviewer reads correct content");
    
    // 授权后：Reviewer 仍然不能写 (ACL 只读)
    ASSERT_FALSE(fileOps->writeFile(REVIEWER_ID, testFile, "hack", 4) > 0, "Reviewer cannot write (ACL is read-only)");
    
    // 撤销授权
    ASSERT_TRUE(fileOps->revokePermission(OWNER_ID, testFile, REVIEWER_ID), "Owner revokes permission");
    ASSERT_FALSE(fileOps->readFile(REVIEWER_ID, testFile, buffer, 100) > 0, "Reviewer cannot read after revoke");

    // 5. 锁定测试 (Locking)
    std::cout << "\n--- Locking Tests ---" << std::endl;
    // 锁定前：Owner 能写
    ASSERT_TRUE(fileOps->writeFile(OWNER_ID, testFile, "update", 6) > 0, "Owner can write before lock");
    
    // 锁定文件
    ASSERT_TRUE(fileOps->setFileLock(OWNER_ID, testFile, true), "Owner locks file");
    
    // 锁定后：Owner 能读，但不能写
    ASSERT_TRUE(fileOps->readFile(OWNER_ID, testFile, buffer, 100) > 0, "Owner can read locked file");
    ASSERT_FALSE(fileOps->writeFile(OWNER_ID, testFile, "hack", 4) > 0, "Owner CANNOT write locked file");
    
    // Admin 应该能写 (根据实现，Admin 拥有所有权限)
    ASSERT_TRUE(fileOps->writeFile(ADMIN_ID, testFile, "admin_edit", 10) > 0, "Admin can write locked file");
    
    // 解锁
    ASSERT_TRUE(fileOps->setFileLock(OWNER_ID, testFile, false), "Owner unlocks file");
    ASSERT_TRUE(fileOps->writeFile(OWNER_ID, testFile, "final", 5) > 0, "Owner can write after unlock");

    std::cout << "\nAll ACL and Locking tests passed successfully!" << std::endl;
    
    // 清理
    remove("test_acl.img");
    return 0;
}
