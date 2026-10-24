#pragma once
#include "stone.h"
#include <cstdint>

static inline bool calcTT(uint8_t input, uint64_t tt) {
    return (tt >> input) & 1ULL;
}
