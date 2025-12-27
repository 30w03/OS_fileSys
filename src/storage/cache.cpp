#include "storage/cache.h"
#include <iostream>

LRUCache::LRUCache(uint32_t capacity) : capacity_(capacity) {}

bool LRUCache::get(uint32_t blockNum, char* buffer) {
    auto it = map_.find(blockNum);
    
    if (it == map_.end()) {
        stats_.misses++;
        return false;
    }
    
    stats_.hits++;
    
    std::memcpy(buffer, it->second->data, Config::BLOCK_SIZE);
    
    entries_.splice(entries_.begin(), entries_, it->second);
    
    return true;
}

void LRUCache::put(uint32_t blockNum, const char* buffer) {
    auto it = map_.find(blockNum);
    
    if (it != map_.end()) {
        std::memcpy(it->second->data, buffer, Config::BLOCK_SIZE);
        entries_.splice(entries_.begin(), entries_, it->second);
        return;
    }
    
    if (entries_.size() >= capacity_) {
        uint32_t oldBlockNum = entries_.back().blockNum;
        entries_.pop_back();
        map_.erase(oldBlockNum);
    }
    
    entries_.emplace_front(blockNum, buffer);
    map_[blockNum] = entries_.begin();
}

void LRUCache::clear() {
    entries_.clear();
    map_.clear();
}

void LRUCache::printStats() const {
    std::cout << "Hits: " << stats_.hits << std::endl;
    std::cout << "Misses: " << stats_.misses << std::endl;
    std::cout << "Hit rate: " << stats_.getHitRate() << "%" << std::endl;
}
