#include "storage/bitmap.h"
#include <cmath>

Bitmap::Bitmap(uint32_t size) : size_(size) {
    uint32_t byteCount = (size + 7) / 8;
    bits_.resize(byteCount, 0);
}

void Bitmap::set(uint32_t index, bool value) {
    if (index >= size_) return;
    
    uint32_t byteIndex = getByteIndex(index);
    uint32_t bitOffset = getBitOffset(index);
    
    if (value) {
        bits_[byteIndex] |= (1 << bitOffset);
    } else {
        bits_[byteIndex] &= ~(1 << bitOffset);
    }
}

void Bitmap::clear(uint32_t index) {
    set(index, false);
}

bool Bitmap::test(uint32_t index) const {
    if (index >= size_) return false;
    
    uint32_t byteIndex = getByteIndex(index);
    uint32_t bitOffset = getBitOffset(index);
    
    return (bits_[byteIndex] & (1 << bitOffset)) != 0;
}

int32_t Bitmap::findFree() const {
    for (uint32_t i = 0; i < size_; i++) {
        if (!test(i)) {
            return i;
        }
    }
    return -1;
}

void Bitmap::serialize(char* buffer) const {
    std::memcpy(buffer, bits_.data(), bits_.size());
}

void Bitmap::deserialize(const char* buffer) {
    std::memcpy(bits_.data(), buffer, bits_.size());
}

uint32_t Bitmap::countUsed() const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < size_; i++) {
        if (test(i)) {
            count++;
        }
    }
    return count;
}
