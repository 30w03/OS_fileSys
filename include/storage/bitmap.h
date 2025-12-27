#ifndef BITMAP_H
#define BITMAP_H

#include <cstdint>
#include <cstring>
#include <vector>

class Bitmap {
public:
    Bitmap(uint32_t size);
    ~Bitmap() = default;
    
    void set(uint32_t index, bool value = true);
    void clear(uint32_t index);
    bool test(uint32_t index) const;
    int32_t findFree() const;
    
    void serialize(char* buffer) const;
    void deserialize(const char* buffer);
    
    uint32_t getSize() const { return size_; }
    uint32_t countUsed() const;
    
private:
    uint32_t size_;
    std::vector<uint8_t> bits_;
    
    uint32_t getByteIndex(uint32_t index) const { return index / 8; }
    uint32_t getBitOffset(uint32_t index) const { return index % 8; }
};

#endif
