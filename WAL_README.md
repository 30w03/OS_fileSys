# 预写式日志 (WAL) 与崩溃一致性说明

## 1. 概述
为了解决文件系统在突然断电或崩溃时可能出现的**元数据损坏 (Metadata Corruption)** 问题，我们在底层文件系统中实现了**预写式日志 (Write-Ahead Logging, WAL)** 机制。

WAL 的核心原则是：**在真正修改文件系统数据（如 Inode、目录项、位图）之前，必须先将该操作的“意图”持久化写入到专门的日志区域。**

## 2. 核心数据结构变更

### 2.1 超级块 (Superblock)
**文件**: `include/filesystem/superblock.h`

我们在超级块中划分了专门的日志区域，位于 Inode Table 之后，Data Blocks 之前。

*   **`logStartBlock`**: 日志区域的起始块号。
*   **`logSizeBlocks`**: 日志区域的大小（目前固定为 64 块，即 256KB）。

### 2.2 日志条目 (LogEntry)
**文件**: `include/filesystem/wal.h`

每个日志条目是一个固定大小的结构体，包含恢复所需的全部元数据：

*   **`magic`**: 魔数 (`0xWALG`)，用于验证条目有效性。
*   **`txnId`**: 事务 ID，单调递增。
*   **`op`**: 操作类型 (`CREATE_FILE`, `DELETE_FILE`, `MKDIR`, `RMDIR`)。
*   **`inodeId`**: 受影响的 Inode 编号。
*   **`parentInodeId`**: 父目录 Inode 编号（用于目录项恢复）。
*   **`name`**: 文件名或目录名。
*   **`checksum`**: 校验和，防止日志本身损坏导致错误恢复。

## 3. 工作机制

### 3.1 正常写入流程 (Logging)
当用户发起元数据修改操作（如创建文件）时，系统执行以下步骤：

1.  **分配资源**: 在内存中计算出需要分配的 Inode ID。
2.  **写入日志**: 调用 `WALManager::log()`，将操作意图（如 "Create Inode 10, Name 'test.txt'"）写入磁盘上的日志区域。
3.  **执行操作**: 只有日志写入成功后，才去修改真正的 Inode 表、目录数据块和位图。
4.  **检查点 (Checkpoint)**: 当日志区域写满或系统正常卸载时，清空日志指针。

### 3.2 崩溃恢复流程 (Recovery)
系统启动 (`mount`) 时，会自动检查日志区域：

1.  **扫描日志**: 读取所有有效的日志条目，验证魔数和校验和。
2.  **重放 (Replay)**: 按顺序重新执行日志中记录的操作。
    *   **幂等性**: 恢复操作设计为幂等的。例如，如果日志记录了“创建文件 A”，恢复程序会检查文件 A 是否存在。如果已存在，则跳过；如果不存在（说明崩溃发生在写数据前），则重新创建。
    *   **强制分配**: 使用 `BlockManager::forceAllocateInode` 确保恢复出的 Inode ID 与崩溃前一致。
3.  **清理**: 恢复完成后，执行 Checkpoint 清除日志。

## 4. 支持的操作

目前 WAL 覆盖了以下核心元数据操作：

| 操作类型 | 描述 | 恢复行为 |
| :--- | :--- | :--- |
| `CREATE_FILE` | 创建普通文件 | 确保 Inode 被标记为已用，且父目录中存在对应条目。 |
| `DELETE_FILE` | 删除普通文件 | 确保 Inode 被标记为未用，且父目录中移除对应条目。 |
| `MKDIR` | 创建目录 | 确保 Inode (Type=Directory) 被分配，且父目录中存在对应条目。 |
| `RMDIR` | 删除目录 | 确保 Inode 被释放，且父目录中移除对应条目。 |

## 5. 集成与 API

WAL 模块 (`WALManager`) 被集成在 `Filesystem` 类中，并注入到 `FileOps` 和 `DirectoryOps`。

*   **`Filesystem::mount()`**: 自动调用 `walManager_->recover()`。
*   **`FileOps::createFile`**: 自动调用 `walManager_->log(CREATE_FILE, ...)`。
*   **`FileOps::deleteFile`**: 自动调用 `walManager_->log(DELETE_FILE, ...)`。

## 6. 测试与验证

我们提供了专门的测试程序来模拟崩溃场景。

*   **运行测试**: `./test_wal`
*   **测试场景**:
    1.  **Create Recovery**: 写入“创建文件”日志后模拟断电（不写实际数据），重启后验证文件是否出现。
    2.  **Mkdir Recovery**: 写入“创建目录”日志后模拟断电，重启后验证目录是否出现。
    3.  **Delete Recovery**: 写入“删除文件”日志后模拟断电，重启后验证文件是否消失。
