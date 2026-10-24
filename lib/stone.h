#pragma once
#include <iostream>
#include <unordered_map>
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
         std::vector<uint8_t> cacheStack;
         uint32_t cacheStackPointer;
         std::vector<uint64_t> lutStack;
         uint32_t lutStackPointer;
         std::vector<uint8_t> valueStack;
         uint32_t valueStackPointer;

        bool pebbleProcess(Pebble* self);
        std::vector<bool> initInference();

    public:
        Pebble* inputs;
        Pebble* hiddens;
        Pebble* outputs;
        Pebble* pebbles;

        std::unordered_map<uint64_t, uint64_t> cacheTransMap;

        std::vector<uint8_t> cache; // 00 -> not cached ; 10 -> cached 0 ; 11 -> cached 1
        std::vector<uint8_t> value;

        StbHeader stbHeader;

        Stone(const std::string& path);

        std::vector<bool> inference(std::vector<bool> &input);

        std::string to_verilog();

        std::string emit_verilog(const std::string& path);
};
