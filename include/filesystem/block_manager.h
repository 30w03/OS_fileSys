#ifndef BLOCK_MANAGER_H
#define BLOCK_MANAGER_H

#include "filesystem/disk.h"
#include "filesystem/inode.h"
#include "filesystem/superblock.h"
#include "storage/cache.h"
#include <memory>
#include <vector>
#include <cstdint>

// 定义常量
constexpr uint32_t SUPERBLOCK_MAGIC = 0x53465350; // Magic: "SFS0"

class BlockManager {
public:
    // 构造函数
    explicit BlockManager(std::shared_ptr<Disk> disk, uint32_t cacheSize = 64);
    ~BlockManager();

    // --- 核心生命周期 ---
    // 格式化磁盘：初始化 Superblock, Bitmaps, InodeTable
    bool format(uint32_t blockSize = 4096, uint32_t totalInodes = 1024);

    // 挂载：读取 Superblock 和 Bitmaps 到内存
    bool mount();

    // 卸载：将内存中的元数据刷回磁盘
    bool unmount();

    // 同步：强制将 Superblock 和 Bitmaps 刷回磁盘 (防止崩溃丢数据)
    bool sync();

    // --- 资源分配 ---
    // 返回 INVALID_INODE 表示失败
    uint32_t allocateInode();
    bool freeInode(uint32_t inodeId);

    // 返回 INVALID_BLOCK 表示失败
    uint32_t allocateBlock();
    bool freeBlock(uint32_t blockId);

    // --- 数据读写 ---
    bool readInode(uint32_t inodeId, Inode& inode);
    bool writeInode(uint32_t inodeId, const Inode& inode);

    // 读写数据块 (buffer 必须至少为 blockSize 大小)
    bool readBlock(uint32_t blockId, char* buffer);
    bool writeBlock(uint32_t blockId, const char* buffer);

    // 清零一个块 (用于分配新块时清除旧数据)
    bool clearBlock(uint32_t blockId);

    // --- 信息查询 ---
    const Superblock& getSuperblock() const { return superblock_; }
    void printStats() const;

private:
    std::shared_ptr<Disk> disk_;
    Superblock superblock_; // 使用 Superblock 替代原来的 SuperBlock

    // 使用 vector<uint8_t> 而不是 vector<bool>
    // 这样可以直接 memcpy 到磁盘 buffer，且避免了 vector<bool> 的特化坑
    std::vector<uint8_t> inodeBitmap_; 
    std::vector<uint8_t> blockBitmap_;

    std::unique_ptr<LRUCache> cache_;
    bool isMounted_;

    // --- 内部辅助函数 ---
    // 计算 Inode 在磁盘上的具体位置
    // 返回 pair<blockId, offsetInBlock>
    std::pair<uint32_t, uint32_t> getInodeLocation(uint32_t inodeId) const;

    // 位图操作辅助
    int findFirstZero(const std::vector<uint8_t>& bitmap);
    void setBit(std::vector<uint8_t>& bitmap, int index, bool value);
    bool getBit(const std::vector<uint8_t>& bitmap, int index) const;
};



#endif // BLOCK_MANAGER_H
