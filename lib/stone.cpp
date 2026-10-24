#include "utils.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include "stone.h"


Stone::Stone(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    long long size = file.tellg();

    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);

    file.read(buffer.data(), size);

    file.close();

    char* data = buffer.data();

    this->stbHeader = ((StbHeader*) data)[0];

    uint32_t pebbleSize = (this->stbHeader.LUTK+1)*sizeof(uint64_t); // bytes(inputs*pointer size + 1 for the tt), IMPORTANT: this assume its an x64 system
    uint32_t inputPebbleSize = sizeof(uint64_t);

    uint32_t bufferSize = size - sizeof(StbHeader); // pebbleSize*(this->stbHeader.amountOfGates-this->stbHeader.amountOfInputs) + inputPebbleSize*this->stbHeader.amountOfInputs; // the size of the buffer
    this->pebbles = (Pebble*) malloc(bufferSize); // Allocate the slab buffer for all of the LUTs

    memcpy(this->pebbles, data + sizeof(StbHeader), bufferSize);

    this->inputs = this->pebbles;
    this->hiddens = (Pebble*)((char*)(this->pebbles)+(inputPebbleSize*this->stbHeader.amountOfInputs)); // Points to the first hidden gate

    for (uint32_t i = 0 ; i < this->stbHeader.amountOfGates-this->stbHeader.amountOfInputs ; i++) {
        Pebble* currentHidden = (Pebble*)((char*)this->hiddens + i * pebbleSize);

        for (uint32_t j = 0 ; j < this->stbHeader.LUTK ; j++) {
            uint64_t id = (uint64_t)currentHidden->inputs[j]; // treat the pointer as a normal id

            if(id < this->stbHeader.amountOfInputs) { // Input pebble
                currentHidden->inputs[j] = &this->inputs[id]; // take the address of the id'th input
            } else { // Hidden pebble
                id -= this->stbHeader.amountOfInputs;
                currentHidden->inputs[j] = &this->hiddens[id] + id*this->stbHeader.LUTK; // take the address of the id'th hidden gate
            }
        }
    }

    uint32_t numHiddens = this->stbHeader.amountOfGates - this->stbHeader.amountOfInputs;
    this->outputs = (Pebble*)((char*)this->hiddens + pebbleSize * (numHiddens - this->stbHeader.amountOfOutputs));

    uint32_t cachedRocks = ((numHiddens*2) + highestPackedBits - 1) / highestPackedBits;
    this->cache.resize(cachedRocks);
}

bool Stone::pebbleProcess(Pebble* self) {
    if (self >= this->hiddens) {
        //caching
        uint32_t index = ((self - this->hiddens) / (this->stbHeader.LUTK+1))*2;

        if (getValue(this->cache, index)) return getValue(this->cache, index+1);

        uint8_t input = 0;
        for(int i = 0 ; i < this->stbHeader.LUTK ; i++) { // pack all of the values of the former gates
            input |= this->pebbleProcess(self->inputs[i]) << i;
        }

        bool res = calcTT(input, self->tt);
        setValue(this->cache, index, 1);
        setValue(this->cache, index + 1, res);
        return res; // calc the tt result
    }
    return getValue(this->value, self - this->inputs); // get the value directly from value if its a input node
}

std::vector<rock> Stone::inference(std::vector<rock> &input) {
    uint32_t pebbleSize = (this->stbHeader.LUTK+1)*sizeof(uint64_t);

    this->value = input;

    uint32_t outputRocks = (this->stbHeader.amountOfOutputs + highestPackedBits - 1) / highestPackedBits;

    std::vector<rock> output(outputRocks); // initiate output vector

    for(int i = 0 ; i < this->stbHeader.amountOfOutputs ; i++) {
        bool res = this->pebbleProcess((Pebble*)((char*)this->outputs + (pebbleSize * i)));
        setValue(output, i, res);
    }

    memset(this->cache.data(), 0, this->cache.size() * sizeof(rock));

    return output;
};
