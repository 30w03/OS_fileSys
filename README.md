# 项目文件夹说明

## 文件夹结构

### apps/
- 包含客户端和服务器的主程序入口文件。
  - `client_main.cpp`: 客户端主程序。
  - `server_main.cpp`: 服务器主程序。

### build/
- 编译输出目录，包含生成的可执行文件和中间文件。
  - `client`: 客户端可执行文件。
  - `server`: 服务器可执行文件。
  - `review_cli`: 命令行工具。
  - `lib*.a`: 各模块的静态库文件（如 `libfilesystem.a`、`libnetwork.a`）。
  - 测试程序：`test_filesystem`、`test_file_ops` 等。
  - `users.dat`: 用户数据文件。

### build_debug/
- 调试模式下的编译输出目录，结构与 `build/` 类似。

### docs/
- 文档目录，目前为空。

### include/
- 项目头文件目录，按模块组织：
  - `client/`: 客户端相关头文件。
  - `common/`: 公共配置头文件。
  - `filesystem/`: 文件系统模块头文件。
  - `network/`: 网络模块头文件。
  - `protocol/`: 协议模块头文件。
  - `review/`: 评审系统模块头文件。
  - `server/`: 服务器相关头文件。
  - `storage/`: 存储模块头文件。
  - `user/`: 用户管理模块头文件。

### src/
- 项目源代码目录，按模块组织：
  - `client/`: 客户端相关代码。
  - `common/`: 公共代码。
  - `filesystem/`: 文件系统模块代码。
  - `network/`: 网络模块代码。
  - `protocol/`: 协议模块代码。
  - `review/`: 评审系统模块代码。
  - `server/`: 服务器相关代码。
  - `storage/`: 存储模块代码。
  - `user/`: 用户管理模块代码。

### tests/
- 测试代码目录，包含各模块的单元测试：
  - `test_filesystem.cpp`: 文件系统测试。
  - `test_file_ops.cpp`: 文件操作测试。
  - `test_network.cpp`: 网络模块测试。
  - 其他测试文件：`test_backup.cpp`、`test_directory.cpp` 等。

---

如需进一步了解，请查看各模块的具体代码或文档。

## 看文件系统时

第一步：看“骨架” (On-Disk Layout)

superblock.h: 整个世界的入口。看它存了什么全局信息（魔数、总块数、Inode表偏移量）。
inode.h: 文件的灵魂。看它怎么索引数据块（就是刚才讨论的那些）。
block.h: 数据的最小单位。
第二步：看“心脏” (The Engine)

block_manager.h / .cpp: 这是最关键的模块。
核心疑问：它是直接读写文件（模拟磁盘），还是通过 LRU 缓存读写？
关键点：看它如何把 Block ID 转换成物理偏移量。
第三步：看“大脑” (High-Level Logic)

filesystem.h: 这时候再看它，你就知道它只是一个指挥官，指挥 Inode 和 Block Manager 干活。
