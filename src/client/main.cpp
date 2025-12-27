#include "client/cli.h"
#include "user/user_manager.h"
#include "review/review_system.h"
#include "filesystem/filesystem.h"
#include <iostream>
#include <memory>

int main() {
    try {
        // 使用实际的 Filesystem 构造函数（只接受 string 路径）
        auto fs = std::make_shared<Filesystem>("review_system.img");
        
        // UserManager 使用默认构造函数
        auto userMgr = std::make_shared<UserManager>();
        
        // ReviewSystem
        auto reviewSys = std::make_shared<ReviewSystem>(fs, userMgr);
        
        // 挂载文件系统
        if (!fs->mount()) {
            std::cerr << "⚠️  Warning: Failed to mount filesystem, creating new one..." << std::endl;
        }
        
        // 尝试加载用户数据
        userMgr->loadFromFile("/system/users.dat");
        
        // 创建并运行 CLI
        CLI cli(userMgr, reviewSys, fs);
        cli.run();
        
        // 保存用户数据
        userMgr->saveToFile("/system/users.dat");
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
