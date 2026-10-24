#pragma once
#include <iostream>
#include <vector>
#include <cstdint>
#include <immintrin.h>

typedef __m128i uint128_t;
typedef __m256i uint256_t;
typedef __m512i uint512_t;

#define GLUE_THREE_HIDDEN(a, b, c) a ## b ## c
#define GLUE_THREE(a, b, c)        GLUE_THREE_HIDDEN(a, b, c)

#define highestPackedBits 128
#define rock GLUE_THREE(uint, highestPackedBits, _t)

typedef struct StbHeader { // This struct must be 16 bytes packed(4,4,4,4)
    uint32_t amountOfInputs;
    uint32_t amountOfOutputs;
    uint32_t amountOfGates;
    uint8_t LUTK;
} StbHeader;

typedef struct Pebble {
    uint64_t tt;
    Pebble* inputs[]; // Inputs must be below tt
} Pebble;


class Stone {
    private:
        Pebble* inputs;
        Pebble* hiddens;
        Pebble* outputs;
        Pebble* pebbles;

        std::vector<rock> cache; // 00 -> not cached ; 10 -> cached 0 ; 11 -> cached 1
        std::vector<rock> value;

        bool pebbleProcess(Pebble* self);

    public:
        StbHeader stbHeader;

        Stone(const std::string& path);

        std::vector<rock> inference(std::vector<rock> &input);

        std::string to_verilog();

        std::string emit_verilog(const std::string& path);
};
