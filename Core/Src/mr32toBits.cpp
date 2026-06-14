/*
 * mr32toBits.cpp
 *
 *  Created on: Aug 12, 2025
 *      Author: michael
 */


#include "mr32toBits.h"

std::string mr32toBits(const void* value, size_t size, int8_t delimit) {
    if (value == nullptr || size == 0 || delimit < 0)
        return std::string();

    const uint8_t* bytePtr = static_cast<const uint8_t*>(value);
    std::string result;
    size_t totalBits = size * 8;
    size_t bitCount = 0;

    // Reserve ungefähre Speichermenge für bessere Performance
    size_t estimatedSize = totalBits;
    if (delimit > 0) {
        estimatedSize += (totalBits / delimit);  // Approximation für Leerzeichen
    }
    result.reserve(estimatedSize);

    for (size_t i = size; i > 0; i--) {
        uint8_t byte = bytePtr[i-1];
        for (int8_t j = 7; j >= 0; j--) {
            result += (byte & (1 << j)) ? '1' : '0';
            bitCount++;

            if (delimit > 0 && (bitCount % delimit == 0) && (bitCount < totalBits)) {
                result += ' ';
            }
        }
    }

    return result;
}
