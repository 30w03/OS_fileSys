#include "filesystem/backup.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

BackupManager::BackupManager(const std::string& diskImage, 
                             const std::string& backupDir)
    : diskImage_(diskImage), backupDir_(backupDir) {
    ensureBackupDirectory();
}

bool BackupManager::ensureBackupDirectory() {
    struct stat st;
    if (stat(backupDir_.c_str(), &st) != 0) {
        // 目录不存在，创建它
        if (mkdir(backupDir_.c_str(), 0755) != 0) {
            std::cerr << "Failed to create backup directory: " << backupDir_ << std::endl;
            return false;
        }
        std::cout << "Created backup directory: " << backupDir_ << std::endl;
    }
    return true;
}

std::string BackupManager::getCurrentTimestamp() const {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    std::ostringstream oss;
    oss << std::put_time(timeinfo, "%Y%m%d_%H%M%S");
    return oss.str();
}

std::string BackupManager::generateBackupName() const {
    return "backup_" + getCurrentTimestamp() + ".img";
}

bool BackupManager::copyFile(const std::string& source, const std::string& dest) {
    std::ifstream src(source, std::ios::binary);
    if (!src.is_open()) {
        std::cerr << "Failed to open source file: " << source << std::endl;
        return false;
    }
    
    std::ofstream dst(dest, std::ios::binary);
    if (!dst.is_open()) {
        std::cerr << "Failed to create destination file: " << dest << std::endl;
        return false;
    }
    
    // 复制文件内容
    dst << src.rdbuf();
    
    if (!dst.good() || !src.good()) {
        std::cerr << "Error during file copy" << std::endl;
        return false;
    }
    
    src.close();
    dst.close();
    
    return true;
}

size_t BackupManager::getFileSize(const std::string& filepath) const {
    struct stat st;
    if (stat(filepath.c_str(), &st) == 0) {
        return st.st_size;
    }
    return 0;
}

bool BackupManager::saveMetadata(const BackupInfo& info) {
    std::string metaFile = backupDir_ + "/" + info.name + ".meta";
    std::ofstream ofs(metaFile);
    
    if (!ofs.is_open()) {
        std::cerr << "Failed to create metadata file: " << metaFile << std::endl;
        return false;
    }
    
    ofs << "name=" << info.name << "\n";
    ofs << "timestamp=" << info.timestamp << "\n";
    ofs << "description=" << info.description << "\n";
    ofs << "size=" << info.size << "\n";
    
    ofs.close();
    return true;
}

BackupInfo BackupManager::loadMetadata(const std::string& backupName) const {
    BackupInfo info;
    std::string metaFile = backupDir_ + "/" + backupName + ".meta";
    
    std::ifstream ifs(metaFile);
    if (!ifs.is_open()) {
        // 如果没有元数据文件，返回基本信息
        info.name = backupName;
        info.timestamp = "Unknown";
        info.description = "No description";
        info.size = getFileSize(backupDir_ + "/" + backupName);
        return info;
    }
    
    std::string line;
    while (std::getline(ifs, line)) {
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        if (key == "name") info.name = value;
        else if (key == "timestamp") info.timestamp = value;
        else if (key == "description") info.description = value;
        else if (key == "size") info.size = std::stoull(value);
    }
    
    ifs.close();
    return info;
}

bool BackupManager::createBackup(const std::string& description) {
    if (!ensureBackupDirectory()) {
        return false;
    }
    
    std::string backupName = generateBackupName();
    std::string backupPath = backupDir_ + "/" + backupName;
    
    std::cout << "Creating backup: " << backupName << std::endl;
    
    // 复制磁盘镜像
    if (!copyFile(diskImage_, backupPath)) {
        std::cerr << "Failed to create backup" << std::endl;
        return false;
    }
    
    // 创建元数据
    BackupInfo info;
    info.name = backupName;
    info.timestamp = getCurrentTimestamp();
    info.description = description.empty() ? "Manual backup" : description;
    info.size = getFileSize(backupPath);
    
    if (!saveMetadata(info)) {
        std::cerr << "Warning: Failed to save backup metadata" << std::endl;
    }
    
    std::cout << "✓ Backup created successfully: " << backupPath << std::endl;
    std::cout << "  Size: " << info.size << " bytes" << std::endl;
    
    return true;
}

std::vector<BackupInfo> BackupManager::listBackups() const {
    std::vector<BackupInfo> backups;
    
    DIR* dir = opendir(backupDir_.c_str());
    if (!dir) {
        std::cerr << "Failed to open backup directory: " << backupDir_ << std::endl;
        return backups;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        
        // 只处理 .img 文件
        if (filename.length() > 4 && 
            filename.substr(filename.length() - 4) == ".img") {
            
            BackupInfo info = loadMetadata(filename);
            backups.push_back(info);
        }
    }
    
    closedir(dir);
    return backups;
}

bool BackupManager::restoreBackup(const std::string& backupName) {
    std::string backupPath = backupDir_ + "/" + backupName;
    
    // 检查备份文件是否存在
    struct stat st;
    if (stat(backupPath.c_str(), &st) != 0) {
        std::cerr << "Backup file not found: " << backupPath << std::endl;
        return false;
    }
    
    std::cout << "Restoring from backup: " << backupName << std::endl;
    
    // 创建当前磁盘镜像的临时备份
    std::string tempBackup = diskImage_ + ".tmp";
    if (!copyFile(diskImage_, tempBackup)) {
        std::cerr << "Failed to create temporary backup" << std::endl;
        return false;
    }
    
    // 恢复备份
    if (!copyFile(backupPath, diskImage_)) {
        std::cerr << "Failed to restore backup" << std::endl;
        // 恢复原文件
        copyFile(tempBackup, diskImage_);
        remove(tempBackup.c_str());
        return false;
    }
    
    // 删除临时文件
    remove(tempBackup.c_str());
    
    std::cout << "✓ Backup restored successfully" << std::endl;
    return true;
}

bool BackupManager::deleteBackup(const std::string& backupName) {
    std::string backupPath = backupDir_ + "/" + backupName;
    std::string metaPath = backupPath + ".meta";
    
    // 删除备份文件
    if (remove(backupPath.c_str()) != 0) {
        std::cerr << "Failed to delete backup: " << backupPath << std::endl;
        return false;
    }
    
    // 删除元数据文件（如果存在）
    remove(metaPath.c_str());
    
    std::cout << "✓ Backup deleted: " << backupName << std::endl;
    return true;
}

void BackupManager::printBackups() const {
    std::vector<BackupInfo> backups = listBackups();
    
    if (backups.empty()) {
        std::cout << "No backups found." << std::endl;
        return;
    }
    
    std::cout << "\n=== Available Backups ===" << std::endl;
    std::cout << std::left << std::setw(30) << "Name" 
              << std::setw(20) << "Timestamp"
              << std::setw(15) << "Size (bytes)"
              << "Description" << std::endl;
    std::cout << std::string(90, '-') << std::endl;
    
    for (const auto& backup : backups) {
        std::cout << std::left << std::setw(30) << backup.name
                  << std::setw(20) << backup.timestamp
                  << std::setw(15) << backup.size
                  << backup.description << std::endl;
    }
}
