#pragma once
#include "stone.h"
#include <cstdint>

static inline bool calcTT(uint8_t input, uint64_t tt) {
    return (tt >> input) & 1ULL;
}

// In utils.h[cite: 3]
static inline void setValue(std::vector<rock> &Value, uint64_t index, bool value) {
    uint64_t rockIdx = index / highestPackedBits;
    uint64_t bitInRock = index % highestPackedBits;

    uint64_t* lanes = reinterpret_cast<uint64_t*>(&Value[rockIdx]);
    uint64_t laneIdx = bitInRock >> 6;         // bitInRock / 64
    uint64_t bitMask = 1ULL << (bitInRock & 63); // 1ULL << (bitInRock % 64)

    lanes[laneIdx] = (lanes[laneIdx] & ~bitMask) | (-static_cast<uint64_t>(value) & bitMask);
}

static inline bool getValue(std::vector<rock> &Value, uint64_t index) {
    uint64_t rockIdx = index / highestPackedBits;
    uint64_t bitInRock = index % highestPackedBits;

    const uint64_t* lanes = reinterpret_cast<const uint64_t*>(&Value[rockIdx]);
    return (lanes[bitInRock >> 6] >> (bitInRock & 63)) & 1ULL;
}
