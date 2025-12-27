#ifndef BACKUP_H
#define BACKUP_H

#include <string>
#include <vector>
#include <ctime>

struct BackupInfo {
    std::string name;           // 备份文件名
    std::string timestamp;      // 时间戳
    std::string description;    // 备份描述
    size_t size;                // 文件大小（字节）
    
    BackupInfo() : size(0) {}
    
    BackupInfo(const std::string& n, const std::string& ts, 
               const std::string& desc, size_t s)
        : name(n), timestamp(ts), description(desc), size(s) {}
};

class BackupManager {
public:
    explicit BackupManager(const std::string& diskImage, 
                          const std::string& backupDir = "backups");
    ~BackupManager() = default;
    
    // 创建备份
    bool createBackup(const std::string& description = "");
    
    // 列出所有备份
    std::vector<BackupInfo> listBackups() const;
    
    // 恢复备份
    bool restoreBackup(const std::string& backupName);
    
    // 删除备份
    bool deleteBackup(const std::string& backupName);
    
    // 打印备份列表
    void printBackups() const;
    
private:
    std::string diskImage_;     // 原始磁盘镜像路径
    std::string backupDir_;     // 备份目录路径
    
    // 生成备份文件名（基于时间戳）
    std::string generateBackupName() const;
    
    // 获取当前时间戳字符串
    std::string getCurrentTimestamp() const;
    
    // 确保备份目录存在
    bool ensureBackupDirectory();
    
    // 复制文件
    bool copyFile(const std::string& source, const std::string& dest);
    
    // 获取文件大小
    size_t getFileSize(const std::string& filepath) const;
    
    // 保存/加载备份元数据
    bool saveMetadata(const BackupInfo& info);
    BackupInfo loadMetadata(const std::string& backupName) const;
};

#endif // BACKUP_H
