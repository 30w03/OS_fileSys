#ifndef FILE_H
#define FILE_H

#include <string>
#include <memory>
#include "filesystem/block.h"
#include "filesystem/inode.h"

// 表示文件的抽象类，提供对文件内容的读写操作
class File {
public:
    // 构造函数：初始化文件对象，绑定 BlockManager 和指定的 Inode 编号
    File(std::shared_ptr<BlockManager> bm, uint32_t inodeNum);
    
    // 析构函数：确保文件的元数据（Inode）被保存
    ~File();
    
    // 读取文件内容到缓冲区
    // 参数：
    // - buffer: 用于存储读取数据的缓冲区
    // - size: 要读取的字节数
    // - offset: 文件中的偏移量
    // 返回值：实际读取的字节数，或错误码
    int32_t read(char* buffer, uint32_t size, uint32_t offset);
    
    // 写入数据到文件
    // 参数：
    // - buffer: 要写入的数据缓冲区
    // - size: 要写入的字节数
    // - offset: 文件中的偏移量
    // 返回值：实际写入的字节数，或错误码
    int32_t write(const char* buffer, uint32_t size, uint32_t offset);
    
    // 追加数据到文件末尾
    // 参数：
    // - buffer: 要追加的数据缓冲区
    // - size: 要追加的字节数
    // 返回值：实际追加的字节数，或错误码
    int32_t append(const char* buffer, uint32_t size);
    
    // 截断文件到指定大小
    // 参数：
    // - newSize: 文件的新大小
    // 返回值：操作是否成功
    bool truncate(uint32_t newSize);
    
    // 获取文件的当前大小
    // 返回值：文件的大小（字节数）
    uint32_t getSize() const;
    
    // 获取文件的 Inode 信息
    // 参数：
    // - inode: 用于存储 Inode 信息的引用
    // 返回值：操作是否成功
    bool getInode(Inode& inode) const;
    
    // 将文件的元数据（Inode）保存到磁盘
    // 返回值：操作是否成功
    bool flush();
    
private:
    std::shared_ptr<BlockManager> blockManager_; // 管理磁盘块的 BlockManager
    uint32_t inodeNum_; // 文件对应的 Inode 编号
    Inode inode_; // 文件的 Inode 元数据
    bool dirty_; // 标记文件的元数据是否被修改
    
    // 获取逻辑块号对应的物理块号
    // 参数：
    // - logicalBlock: 文件中的逻辑块号
    // 返回值：对应的物理块号
    uint32_t getBlockNum(uint32_t logicalBlock);
    
    // 为指定的逻辑块分配一个物理块
    // 参数：
    // - logicalBlock: 文件中的逻辑块号
    // 返回值：操作是否成功
    bool allocateBlock(uint32_t logicalBlock);
    
    // 从磁盘加载文件的 Inode 信息到内存
    void loadInode();
    
    // 将文件的 Inode 信息保存到磁盘
    void saveInode();
};

#endif