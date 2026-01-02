#ifndef WAL_H
#define WAL_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "filesystem/block_manager.h"

// 日志操作类型
enum class LogOp : uint8_t {
    NONE = 0,
    CREATE_FILE,
    DELETE_FILE,
    MKDIR,
    RMDIR,
    UPDATE_INODE // 用于元数据更新
};

// 日志条目结构 (固定大小，方便读取)
struct LogEntry {
    uint32_t magic;         // 魔数，用于验证有效性 (0xWALMAGIC)
    uint32_t txnId;         // 事务ID
    LogOp op;               // 操作类型
    uint32_t inodeId;       // 涉及的 Inode
    uint32_t parentInodeId; // 父目录 Inode (用于 Create/Delete)
    char name[64];          // 文件名 (用于 Create/Delete)
    uint64_t timestamp;     // 时间戳
    uint32_t checksum;      // 校验和
};

class FileOps;
class DirectoryOps;

class WALManager {
public:
    WALManager(std::shared_ptr<BlockManager> bm, uint32_t startBlock, uint32_t sizeBlocks);
    
    // 初始化日志系统
    bool init();
    
    // 写入日志
    bool log(LogOp op, uint32_t inodeId, uint32_t parentInodeId = 0, const std::string& name = "");
    
    // 检查点：清除日志 (实际上是将日志指针重置)
    bool checkpoint();
    
    // 恢复：重放日志
    bool recover(std::shared_ptr<FileOps> fileOps, std::shared_ptr<DirectoryOps> dirOps);

    void setLoggingEnabled(bool enabled) { loggingEnabled_ = enabled; }

    // 获取下一个事务ID
    uint32_t getNextTxnId() const { return nextTxnId_; }

private:
    std::shared_ptr<BlockManager> bm_;
    uint32_t startBlock_;
    uint32_t sizeBlocks_;
    uint32_t currentOffset_; // 当前写入位置 (字节偏移)
    uint32_t nextTxnId_;
    bool loggingEnabled_ = true;

    // 辅助函数
    void writeEntry(const LogEntry& entry);
    std::vector<LogEntry> readAllEntries();
    uint32_t calculateChecksum(const LogEntry& entry);
};

#endif // WAL_H
