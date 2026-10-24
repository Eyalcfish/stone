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

	this->cache.resize(numHiddens*2);

	this->initInference();
}

bool Stone::pebbleProcess(Pebble* self) {
	if (self >= this->hiddens) {
		//caching
		uint32_t index = ((self - this->hiddens) / (this->stbHeader.LUTK+1)) * 2;

		if (this->cache[index]) {
			//log
			this->lutStack.push_back(0);
			this->lutStack.push_back(cacheTransMap[(uint64_t)self]);

			return this->cache[index+1];
		}

		if (self->tt == 0 || self->tt == -1) {
			this->lutStack.push_back(0);
			this->lutStack.push_back(this->stbHeader.amountOfInputs + (self->tt&1)); // get the false(at place [sizeofinputs]) if tt 0, get [sizeofinputs+1] if tt 1
			return self->tt;
		}

		uint8_t input = 0;
		for(int i = 0 ; i < this->stbHeader.LUTK ; i++) { // pack all of the values of the former gates
			input |= this->pebbleProcess(self->inputs[i]) << i;
		}

		bool res = calcTT(input, self->tt);

		// log
		this->lutStack.push_back(self->tt); // push the tt of the current calculation
		this->cacheTransMap[(uint64_t)self] = this->cacheStack.size(); // for future tracing, save the node's pointer with the index of the cached result
		this->cacheStack.push_back(res); // push the result of this node's into the cache(mostly used to raise the size)

		this->cache[index++] = true;
		this->cache[index] = res;
		return res; // calc the tt result
	} // -1 >> 1

	//log
	this->lutStack.push_back(0); // lookup from cache
	this->lutStack.push_back(self - this->inputs); // which input to read from

	return this->value[self - this->inputs]; // get the value directly from value if its a input node
}

std::vector<bool> Stone::initInference() {
	uint32_t pebbleSize = (this->stbHeader.LUTK+1)*sizeof(uint64_t);

	this->value.resize(this->stbHeader.amountOfInputs);
	for(int i = 0 ; i < this->stbHeader.amountOfInputs ; i++) {
		//log
		this->cacheStack.push_back(0); // start by saving all of the inputs in the cache(for logging in here)
	}
	this->cacheStack.push_back(false);
	this->cacheStack.push_back(true);

	std::vector<bool> output(this->stbHeader.amountOfOutputs); // initiate output vector

	for(int i = 0 ; i < this->stbHeader.amountOfOutputs ; i++) {
		bool res = this->pebbleProcess((Pebble*)((char*)this->outputs + (pebbleSize * i)));
		output[i] = res;
	}

	std::fill(this->cache.begin(), this->cache.end(), false);

	this->valueStack.clear();
	this->cacheStack.clear();
	std::unordered_map<uint64_t, uint64_t>().swap(this->cacheTransMap); // completely frees the map by keeping it out of context

	return output;
};

std::vector<bool> Stone::inference(std::vector<bool> &input) {
	this->cacheStack.clear();
	this->valueStack.clear();

	for (uint32_t i = 0; i < this->stbHeader.amountOfInputs; i++) {
		this->cacheStack.push_back(input[i]);
	}
	this->cacheStack.push_back(false); // index: amountOfInputs (constant 0)
	this->cacheStack.push_back(true);  // index: amountOfInputs + 1 (constant 1)
	this->cacheStackPointer = this->cacheStack.size();

	for (size_t pc = 0; pc < this->lutStack.size(); pc++) {
		uint64_t opcode = this->lutStack[pc];

		if (opcode == 0) { // load a value from the cache
			uint64_t cacheIdx = this->lutStack[++pc]; // read the index of the specified cache value
			this->valueStack[this->valueStackPointer++] = this->cacheStack[cacheIdx]; // load the cached value onto the value stack
		} else { // run an tt on the last K values
			uint8_t packedInput = 0;
			size_t baseIdx = this->valueStack.size() - this->stbHeader.LUTK; // get the value of the first input

			for (uint8_t k = 0; k < this->stbHeader.LUTK; k++) { // pack the values for the calcTT function
				packedInput |= (this->valueStack[baseIdx + k] << k);
			}

			bool res = calcTT(packedInput, opcode); // calculate the value

			this->valueStackPointer = baseIdx; // reset to the "sp"

			this->valueStack[this->valueStackPointer++] = res; // push the results into the value stack
			this->cacheStack[this->cacheStackPointer++] = res; // push the results into the cache stack
		}
	}

	std::vector<bool> output(this->stbHeader.amountOfOutputs);
	for (uint32_t i = 0; i < this->stbHeader.amountOfOutputs; i++) { // peel the final values from the bottom of the value stack
		output[i] = this->valueStack[i];
	}

	return output;
}
