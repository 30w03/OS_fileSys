#ifndef DISK_H
#define DISK_H

#include <string>
#include <cstdint>
#include <fstream>
#include "common/config.h"

class Disk {
public:
    Disk(const std::string& path);
    ~Disk();
    
    bool format();
    bool readBlock(uint32_t blockNum, char* buffer);
    bool writeBlock(uint32_t blockNum, const char* buffer);
    
    struct Stats {
        uint64_t reads;
        uint64_t writes;
        
        Stats() : reads(0), writes(0) {}
    };
    
    Stats getStats() const { return stats_; }
    void printStats() const;
    
    std::string getPath() const { return path_; }
    
private:
    std::string path_;
    std::fstream file_;
    Stats stats_;
    
    bool open();
    void close();
};

#endif