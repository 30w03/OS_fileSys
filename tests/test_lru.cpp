#include "storage/disk.h"
#include "storage/lru_cache.h"
#include <iostream>
#include <cstring>
#include <cassert>

void testDisk() {
    std::cout << "=== Testing Disk ===" << std::endl;
    
    Disk disk("test_disk.img");
    assert(disk.format());
    
    char writeBuffer[Config::BLOCK_SIZE];
    std::memset(writeBuffer, 'A', Config::BLOCK_SIZE);
    assert(disk.writeBlock(100, writeBuffer));
    
    char readBuffer[Config::BLOCK_SIZE];
    assert(disk.readBlock(100, readBuffer));
    assert(std::memcmp(writeBuffer, readBuffer, Config::BLOCK_SIZE) == 0);
    
    std::cout << "Disk test passed! Reads: " << disk.getReadCount() 
              << ", Writes: " << disk.getWriteCount() << std::endl;
}

void testLRU() {
    std::cout << "\n=== Testing LRU Cache ===" << std::endl;
    
    Disk disk("test_disk.img");
    disk.load();
    
    LRUCache cache(4);
    
    cache.setDiskCallbacks(
        [&disk](uint32_t blockNum, char* buffer) {
            return disk.readBlock(blockNum, buffer);
        },
        [&disk](uint32_t blockNum, const char* buffer) {
            return disk.writeBlock(blockNum, buffer);
        }
    );
    
    char buffer[Config::BLOCK_SIZE];
    
    cache.get(1, buffer);
    cache.get(2, buffer);
    cache.get(3, buffer);
    cache.get(4, buffer);
    cache.get(1, buffer);
    cache.get(2, buffer);
    cache.get(5, buffer);
    
    auto stats = cache.getStats();
    std::cout << "Cache hits: " << stats.hits << std::endl;
    std::cout << "Cache misses: " << stats.misses << std::endl;
    std::cout << "Cache evictions: " << stats.evictions << std::endl;
    std::cout << "Hit rate: " << (stats.getHitRate() * 100) << "%" << std::endl;
    
    assert(stats.hits == 2);
    assert(stats.misses == 5);
    assert(stats.evictions == 1);
    
    std::cout << "LRU Cache test passed!" << std::endl;
}

int main() {
    testDisk();
    testLRU();
    
    std::cout << "\n✅ All storage tests passed!" << std::endl;
    return 0;
}
