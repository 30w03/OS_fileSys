#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include <cstdint>
#include <unordered_map>
#include <list>
#include <cstring>
#include "common/config.h"

struct CacheEntry {
    uint32_t blockNum;
    char data[Config::BLOCK_SIZE];
    
    CacheEntry(uint32_t num, const char* buffer) : blockNum(num) {
        std::memcpy(data, buffer, Config::BLOCK_SIZE);
    }
};

class LRUCache {
public:
    LRUCache(uint32_t capacity);
    ~LRUCache() = default;
    
    bool get(uint32_t blockNum, char* buffer);
    void put(uint32_t blockNum, const char* buffer);
    void clear();
    
    struct Stats {
        uint64_t hits;
        uint64_t misses;
        
        Stats() : hits(0), misses(0) {}
        
        double getHitRate() const {
            uint64_t total = hits + misses;
            return total > 0 ? (double)hits / total * 100.0 : 0.0;
        }
    };
    
    Stats getStats() const { return stats_; }
    void printStats() const;
    
private:
    uint32_t capacity_;
    std::list<CacheEntry> entries_;
    std::unordered_map<uint32_t, std::list<CacheEntry>::iterator> map_;
    Stats stats_;
};

#endif
