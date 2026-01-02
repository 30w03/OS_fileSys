# 访问控制列表 (ACL) 与文件锁定机制说明

## 1. 概述
为了支持同行评审系统中的“盲审”和“论文锁定”需求，我们在底层文件系统中实现了基于 Inode 的访问控制列表 (ACL) 和文件锁定机制。这确保了数据安全不仅仅依赖于业务逻辑，而是由底层存储强制执行。

## 2. 核心数据结构变更
**文件**: `include/filesystem/inode.h`

我们修改了 `Inode` 结构体（保持 128 字节对齐），引入了以下字段：

*   **`acl_uids[4]`**: 一个包含 4 个用户 ID 的数组。
    *   用途：存储被显式授予**只读权限**的用户（如审稿人）。
    *   限制：目前支持每个文件最多 4 个额外的读者。
*   **`flags`**: 32位标志位。
    *   `INODE_FLAG_LOCKED (0x00000001)`: 当此位被设置时，文件进入**锁定状态**。

## 3. 权限模型
文件系统操作 (`FileOps::checkPermission`) 遵循以下优先级逻辑：

1.  **Admin (uid=1)**:
    *   拥有所有文件的完全读写权限，忽略 ACL 和锁定状态。
    *   用于系统维护和强制操作。

2.  **Owner (文件拥有者)**:
    *   **正常状态**: 拥有读 (Read) 和写 (Write) 权限。
    *   **锁定状态**: 降级为**只读 (Read-Only)**。无法修改或删除文件。

3.  **ACL (授权用户)**:
    *   如果在 `acl_uids` 列表中找到用户 ID：拥有**只读 (Read-Only)** 权限。
    *   ACL 用户永远无法获得写权限。

4.  **Others (其他用户)**:
    *   无任何权限 (Access Denied)。

## 4. 新增 API 接口
**文件**: `src/filesystem/file_ops.cpp`

| 函数名 | 描述 | 调用者 |
| :--- | :--- | :--- |
| `grantPermission(uid, path, targetUid)` | 将 `targetUid` 添加到文件的 ACL 列表中。 | ReviewSystem (分配审稿人时) |
| `revokePermission(uid, path, targetUid)` | 从 ACL 列表中移除 `targetUid`。 | ReviewSystem (移除审稿人时) |
| `setFileLock(uid, path, locked)` | 设置或清除文件的锁定标志。 | ReviewSystem (进入/退出审稿阶段) |
| `checkPermission(uid, inode, mode)` | 内部核心函数，验证用户是否有权执行操作。 | 所有读写操作自动调用 |

## 5. 业务流程集成
ACL 机制已深度集成到 `ReviewSystem` 中：

### 5.1 论文提交 (`submitPaper`)
*   **操作**: 调用 `fileOps->createFile(authorId, path)`。
*   **效果**: 文件 Owner 被设置为**作者本人**。作者可以自由修改（上传新版本）。

### 5.2 分配审稿人 (`assignReviewer`)
*   **操作 1**: 调用 `fileOps->setFileLock(1, path, true)`。
    *   **效果**: 论文被锁定。作者（Owner）变为只读，无法再上传修改版或删除论文。
*   **操作 2**: 调用 `fileOps->grantPermission(1, path, reviewerId)`。
    *   **效果**: 审稿人被添加到 ACL。审稿人可以读取（下载）论文，但不能修改。

### 5.3 论文下载 (`handleDownloadPaper`)
*   **操作**: 服务器响应下载请求时，调用 `filesystem_->readFile(path, data)`。
*   **效果**: 底层自动触发 `checkPermission`。
    *   如果是作者：允许下载。
    *   如果是审稿人：允许下载（通过 ACL）。
    *   如果是路人：拒绝访问。

## 6. 测试与验证
我们提供了专门的测试套件来验证安全性。

*   **运行测试**: `./test_acl_lock`
*   **测试覆盖**:
    *   Owner 读写权限。
    *   非授权用户拒绝访问。
    *   ACL 授权与撤销。
    *   文件锁定对 Owner 写权限的限制。
    *   Admin 超级权限。
