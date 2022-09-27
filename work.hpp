#pragma once
#include <cstdint>

inline std::uint64_t workload(std::uint64_t value) {
    for (int i = 0; i < 8192; ++i) {
        value ^= value >> 12;
        value ^= value << 25;
        value ^= value >> 27;
        value *= 0x2545f4914f6cdd1dULL;
    }
    return value;
}
