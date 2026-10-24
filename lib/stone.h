#pragma once
#include <iostream>
#include <vector>
#include <cstdint>
#include <immintrin.h>

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

        std::vector<bool> cache; // 00 -> not cached ; 10 -> cached 0 ; 11 -> cached 1
        std::vector<bool> value;

        bool pebbleProcess(Pebble* self);

    public:
        StbHeader stbHeader;

        Stone(const std::string& path);

        std::vector<bool> inference(std::vector<bool> &input);

        std::string to_verilog();

        std::string emit_verilog(const std::string& path);
};
