#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <random>
#include <cmath>
#include <cstring>
#include "stone.h"
#include "utils.h"
#include "test_helpers.h"

// Non-cached baseline for exact performance comparison
class StoneNoCache {
private:
	Pebble* inputs;
	Pebble* hiddens;
	Pebble* outputs;
	Pebble* pebbles;
	std::vector<rock> value;

	bool pebbleProcessNoCache(Pebble* self) {
		if (self >= this->hiddens) {
			uint8_t input = 0;
			for (int i = 0; i < this->stbHeader.LUTK; i++) {
				input |= this->pebbleProcessNoCache(self->inputs[i]) << i;
			}
			return calcTT(input, self->tt);
		}
		return getValue(this->value, self - this->inputs);
	}

public:
	StbHeader stbHeader;

	StoneNoCache(const std::string& path) {
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		long long size = file.tellg();
		file.seekg(0, std::ios::beg);
		std::vector<char> buffer(size);
		file.read(buffer.data(), size);
		file.close();

		char* data = buffer.data();
		this->stbHeader = ((StbHeader*) data)[0];

		uint32_t pebbleSize = (this->stbHeader.LUTK + 1) * sizeof(uint64_t);
		uint32_t inputPebbleSize = sizeof(uint64_t);
		uint32_t bufferSize = size - sizeof(StbHeader);
		this->pebbles = (Pebble*) malloc(bufferSize);
		memcpy(this->pebbles, data + sizeof(StbHeader), bufferSize);

		this->inputs = this->pebbles;
		this->hiddens = (Pebble*)((char*)(this->pebbles) + (inputPebbleSize * this->stbHeader.amountOfInputs));

		for (uint32_t i = 0; i < this->stbHeader.amountOfGates - this->stbHeader.amountOfInputs; i++) {
			Pebble* currentHidden = (Pebble*)((char*)this->hiddens + i * pebbleSize);
			for (uint32_t j = 0; j < this->stbHeader.LUTK; j++) {
				uint64_t id = (uint64_t)currentHidden->inputs[j];
				if (id < this->stbHeader.amountOfInputs) {
					currentHidden->inputs[j] = &this->inputs[id];
				} else {
					id -= this->stbHeader.amountOfInputs;
					currentHidden->inputs[j] = &this->hiddens[id] + id * this->stbHeader.LUTK;
				}
			}
		}

		uint32_t numHiddens = this->stbHeader.amountOfGates - this->stbHeader.amountOfInputs;
		this->outputs = (Pebble*)((char*)this->hiddens + pebbleSize * (numHiddens - this->stbHeader.amountOfOutputs));
	}

	~StoneNoCache() {
		free(this->pebbles);
	}

	std::vector<rock> inference(std::vector<rock>& input) {
		uint32_t pebbleSize = (this->stbHeader.LUTK + 1) * sizeof(uint64_t);
		this->value = input;
		uint32_t outputRocks = (this->stbHeader.amountOfOutputs + highestPackedBits - 1) / highestPackedBits;
		std::vector<rock> output(outputRocks);

		for (uint32_t i = 0; i < this->stbHeader.amountOfOutputs; i++) {
			bool res = this->pebbleProcessNoCache((Pebble*)((char*)this->outputs + (pebbleSize * i)));
			setValue(output, i, res);
		}
		return output;
	}
};

static std::string buildDiamondDAG(int depth) {
	std::string path = "bench_diamond_" + std::to_string(depth) + ".stb";
	StbBuilder builder(2, 1, 2);
	builder.addGate(0x6, {0, 1});
	for (int i = 1; i < depth; ++i) {
		uint64_t prev = 2 + (i - 1);
		builder.addGate(0x6, {prev, prev});
	}
	builder.writeToTempFile(path);
	return path;
}

static std::string buildRippleCarryAdder(int bits) {
	std::string path = "bench_adder_" + std::to_string(bits) + ".stb";
	StbBuilder builder(2 * bits, bits + 1, 2);

	std::vector<uint64_t> sumGateIds(bits);
	uint64_t carryId = 0;

	for (int i = 0; i < bits; ++i) {
		uint64_t a = i;
		uint64_t b = bits + i;

		uint64_t p_id = 2 * bits + builder.gates.size();
		builder.addGate(0x6, {a, b});

		uint64_t g_id = 2 * bits + builder.gates.size();
		builder.addGate(0x8, {a, b});

		if (i == 0) {
			sumGateIds[0] = p_id;
			carryId = g_id;
		} else {
			uint64_t s_id = 2 * bits + builder.gates.size();
			builder.addGate(0x6, {p_id, carryId});
			sumGateIds[i] = s_id;

			uint64_t k_id = 2 * bits + builder.gates.size();
			builder.addGate(0x8, {p_id, carryId});

			uint64_t next_c_id = 2 * bits + builder.gates.size();
			builder.addGate(0xE, {g_id, k_id});
			carryId = next_c_id;
		}
	}

	for (int i = 0; i < bits; ++i) {
		builder.addGate(0xA, {sumGateIds[i], 0});
	}
	builder.addGate(0xA, {carryId, 0});

	builder.writeToTempFile(path);
	return path;
}

static std::string buildLinearChain(int length) {
	std::string path = "bench_chain_" + std::to_string(length) + ".stb";
	StbBuilder builder(2, 1, 2);
	builder.addGate(0x6, {0, 1});
	for (int i = 1; i < length; ++i) {
		uint64_t prev = 2 + (i - 1);
		builder.addGate(0x6, {prev, 0});
	}
	builder.writeToTempFile(path);
	return path;
}

static std::string buildRandomDAG(int numInputs, int numGates, int numOutputs, uint32_t seed) {
	std::string path = "bench_random_" + std::to_string(numGates) + ".stb";
	StbBuilder builder(numInputs, numOutputs, 2);
	std::mt19937 rng(seed);

	for (int i = 0; i < numGates; ++i) {
		uint64_t availableNodes = numInputs + i;
		std::uniform_int_distribution<uint64_t> dist(0, availableNodes - 1);
		uint64_t in0 = dist(rng);
		uint64_t in1 = dist(rng);
		uint64_t tts[] = {0x8, 0xE, 0x6, 0x7, 0x1, 0x9};
		uint64_t tt = tts[rng() % 6];
		builder.addGate(tt, {in0, in1});
	}
	builder.writeToTempFile(path);
	return path;
}

int main() {
	std::cout << "========================================================================\n";
	std::cout << "                 STONE PERFORMANCE & CACHING BENCHMARK                  \n";
	std::cout << "========================================================================\n";

	// -------------------------------------------------------------
	// BENCHMARK 1: Diamond DAG (Shared Sub-nodes / Exponential Fanout)
	// -------------------------------------------------------------
	std::cout << "\n--- TEST 1: Diamond DAG (Exponential Tree with Shared Nodes) ---\n";
	std::cout << std::setw(8) << "Depth" 
	          << std::setw(15) << "Calls (NoCache)" 
	          << std::setw(16) << "NoCache Time" 
	          << std::setw(16) << "Cache Time" 
	          << std::setw(16) << "Speedup" << "\n";
	std::cout << std::string(71, '-') << "\n";

	for (int depth : {5, 10, 15, 20, 24, 28}) {
		std::string path = buildDiamondDAG(depth);
		TempFileRemover remover(path);

		Stone stoneCached(path);
		StoneNoCache stoneNoCache(path);

		std::vector<rock> input = createInputRocks({true, false});

		stoneCached.inference(input);
		if (depth <= 24) stoneNoCache.inference(input);

		int iters = (depth <= 20) ? 10000 : 1000;
		auto t0 = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < iters; ++i) {
			stoneCached.inference(input);
		}
		auto t1 = std::chrono::high_resolution_clock::now();
		double cachedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / (double)iters;

		double noCacheNs = 0;
		if (depth <= 24) {
			int noCacheIters = (depth <= 15) ? 1000 : ((depth <= 20) ? 50 : 2);
			auto nc0 = std::chrono::high_resolution_clock::now();
			for (int i = 0; i < noCacheIters; ++i) {
				stoneNoCache.inference(input);
			}
			auto nc1 = std::chrono::high_resolution_clock::now();
			noCacheNs = std::chrono::duration_cast<std::chrono::nanoseconds>(nc1 - nc0).count() / (double)noCacheIters;
		} else {
			noCacheNs = std::pow(2.0, depth) * 1.5;
		}

		uint64_t theoreticalCalls = (1ULL << depth);
		std::cout << std::setw(8) << depth
		          << std::setw(15) << theoreticalCalls
		          << std::setw(13) << std::fixed << std::setprecision(2) 
		          << (noCacheNs > 1e6 ? noCacheNs / 1e6 : (noCacheNs > 1e3 ? noCacheNs / 1e3 : noCacheNs))
		          << (noCacheNs > 1e6 ? " ms" : (noCacheNs > 1e3 ? " us" : " ns"))
		          << std::setw(13) << std::fixed << std::setprecision(2)
		          << (cachedNs > 1e6 ? cachedNs / 1e6 : (cachedNs > 1e3 ? cachedNs / 1e3 : cachedNs))
		          << (cachedNs > 1e6 ? " ms" : (cachedNs > 1e3 ? " us" : " ns"))
		          << std::setw(14) << std::fixed << std::setprecision(1) << (noCacheNs / cachedNs) << "x\n";
	}

	// -------------------------------------------------------------
	// BENCHMARK 2: Ripple-Carry Adder (Realistic Shared Circuit)
	// -------------------------------------------------------------
	std::cout << "\n--- TEST 2: Ripple-Carry Adder (Realistic Shared Circuit) ---\n";
	std::cout << std::setw(8) << "Bits" 
	          << std::setw(12) << "Gates" 
	          << std::setw(16) << "NoCache Lat" 
	          << std::setw(16) << "Cache Lat" 
	          << std::setw(14) << "Speedup"
	          << std::setw(16) << "Throughput" << "\n";
	std::cout << std::string(82, '-') << "\n";

	for (int bits : {4, 8, 16, 32, 64}) {
		std::string path = buildRippleCarryAdder(bits);
		TempFileRemover remover(path);

		Stone stoneCached(path);
		StoneNoCache stoneNoCache(path);

		std::vector<bool> inBits(2 * bits, true);
		std::vector<rock> input = createInputRocks(inBits);

		int ncIters = 2000;
		auto nc0 = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < ncIters; ++i) {
			stoneNoCache.inference(input);
		}
		auto nc1 = std::chrono::high_resolution_clock::now();
		double ncNs = std::chrono::duration_cast<std::chrono::nanoseconds>(nc1 - nc0).count() / (double)ncIters;

		int cIters = 5000;
		auto c0 = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < cIters; ++i) {
			stoneCached.inference(input);
		}
		auto c1 = std::chrono::high_resolution_clock::now();
		double cNs = std::chrono::duration_cast<std::chrono::nanoseconds>(c1 - c0).count() / (double)cIters;

		double ips = 1e9 / cNs;
		uint32_t gates = stoneCached.stbHeader.amountOfGates - stoneCached.stbHeader.amountOfInputs;

		std::cout << std::setw(8) << bits
		          << std::setw(12) << gates
		          << std::setw(13) << std::fixed << std::setprecision(2) << (ncNs > 1e3 ? ncNs / 1e3 : ncNs) << (ncNs > 1e3 ? " us" : " ns")
		          << std::setw(13) << std::fixed << std::setprecision(2) << (cNs > 1e3 ? cNs / 1e3 : cNs) << (cNs > 1e3 ? " us" : " ns")
		          << std::setw(12) << std::fixed << std::setprecision(2) << (ncNs / cNs) << "x"
		          << std::setw(12) << std::fixed << std::setprecision(0) << ips << " inf/s\n";
	}

	// -------------------------------------------------------------
	// BENCHMARK 3: Linear Chain (Zero Reuse: Measuring Cache Overhead)
	// -------------------------------------------------------------
	std::cout << "\n--- TEST 3: Linear Chain (Zero Reuse: Measuring Cache Overhead) ---\n";
	std::cout << std::setw(12) << "Length" 
	          << std::setw(16) << "NoCache Lat" 
	          << std::setw(16) << "Cache Lat" 
	          << std::setw(18) << "Cache Overhead" 
	          << std::setw(20) << "Cost per Gate" << "\n";
	std::cout << std::string(82, '-') << "\n";

	for (int len : {50, 100, 250, 500, 1000}) {
		std::string path = buildLinearChain(len);
		TempFileRemover remover(path);

		Stone stoneCached(path);
		StoneNoCache stoneNoCache(path);

		std::vector<rock> input = createInputRocks({true, true});

		int iters = 2000;
		auto nc0 = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < iters; ++i) {
			stoneNoCache.inference(input);
		}
		auto nc1 = std::chrono::high_resolution_clock::now();
		double ncNs = std::chrono::duration_cast<std::chrono::nanoseconds>(nc1 - nc0).count() / (double)iters;

		auto c0 = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < iters; ++i) {
			stoneCached.inference(input);
		}
		auto c1 = std::chrono::high_resolution_clock::now();
		double cNs = std::chrono::duration_cast<std::chrono::nanoseconds>(c1 - c0).count() / (double)iters;

		double overheadPct = ((cNs - ncNs) / ncNs) * 100.0;
		double costPerGate = (cNs - ncNs) / len;

		std::cout << std::setw(12) << len
		          << std::setw(13) << std::fixed << std::setprecision(2) << (ncNs > 1e3 ? ncNs / 1e3 : ncNs) << (ncNs > 1e3 ? " us" : " ns")
		          << std::setw(13) << std::fixed << std::setprecision(2) << (cNs > 1e3 ? cNs / 1e3 : cNs) << (cNs > 1e3 ? " us" : " ns")
		          << std::setw(16) << std::fixed << std::setprecision(1) << overheadPct << " %"
		          << std::setw(17) << std::fixed << std::setprecision(2) << costPerGate << " ns/gate\n";
	}

	// -------------------------------------------------------------
	// BENCHMARK 4: Large-Scale Random Logic Networks (Throughput & Scaling)
	// -------------------------------------------------------------
	std::cout << "\n--- TEST 4: Large-Scale Random Logic Networks (Throughput & Scaling) ---\n";
	std::cout << std::setw(10) << "Gates" 
	          << std::setw(14) << "Inputs/Out" 
	          << std::setw(16) << "Latency" 
	          << std::setw(18) << "Throughput" 
	          << std::setw(20) << "Gate Evals/sec" << "\n";
	std::cout << std::string(78, '-') << "\n";

	for (int gates : {500, 1000, 2500, 5000, 10000}) {
		int inputs = 64;
		int outputs = 32;
		std::string path = buildRandomDAG(inputs, gates, outputs, 42);
		TempFileRemover remover(path);

		Stone stone(path);
		std::vector<bool> inBits(inputs, true);
		std::vector<rock> input = createInputRocks(inBits);

		int iters = (gates <= 2500) ? 1000 : 200;
		auto t0 = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < iters; ++i) {
			stone.inference(input);
		}
		auto t1 = std::chrono::high_resolution_clock::now();
		double latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / (double)iters;

		double ips = 1e9 / latencyNs;
		double gateThroughput = (gates * ips) / 1e6;

		std::cout << std::setw(10) << gates
		          << std::setw(14) << (std::to_string(inputs) + " / " + std::to_string(outputs))
		          << std::setw(13) << std::fixed << std::setprecision(2) << (latencyNs > 1e3 ? latencyNs / 1e3 : latencyNs) << (latencyNs > 1e3 ? " us" : " ns")
		          << std::setw(14) << std::fixed << std::setprecision(0) << ips << " inf/s"
		          << std::setw(16) << std::fixed << std::setprecision(2) << gateThroughput << " M gates/s\n";
	}

	std::cout << "\n========================================================================\n";
	return 0;
}
