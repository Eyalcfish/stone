#pragma once
#include "stone.h"
#include "utils.h"
#include <vector>
#include <string>
#include <fstream>
#include <cstdint>
#include <cstdio>

class StbBuilder {
public:
	uint32_t amountOfInputs;
	uint32_t amountOfOutputs;
	uint8_t lutK;

	struct Gate {
		uint64_t tt;
		std::vector<uint64_t> inputs;
	};

	std::vector<Gate> gates;

	StbBuilder(uint32_t inputs, uint32_t outputs, uint8_t k)
		: amountOfInputs(inputs), amountOfOutputs(outputs), lutK(k) {}

	void addGate(uint64_t tt, const std::vector<uint64_t>& gateInputs) {
		gates.push_back({tt, gateInputs});
	}

	std::vector<uint8_t> buildBinary() const {
		std::vector<uint8_t> buffer;

		StbHeader header;
		header.amountOfInputs = amountOfInputs;
		header.amountOfOutputs = amountOfOutputs;
		header.amountOfGates = amountOfInputs + static_cast<uint32_t>(gates.size());
		header.LUTK = lutK;

		const uint8_t* headerBytes = reinterpret_cast<const uint8_t*>(&header);
		buffer.insert(buffer.end(), headerBytes, headerBytes + sizeof(StbHeader));

		// Input pebbles dummy entries (amountOfInputs * 8 bytes)
		uint64_t dummy = 0;
		const uint8_t* dummyBytes = reinterpret_cast<const uint8_t*>(&dummy);
		for (uint32_t i = 0; i < amountOfInputs; ++i) {
			buffer.insert(buffer.end(), dummyBytes, dummyBytes + sizeof(uint64_t));
		}

		// Hidden pebbles
		for (const auto& gate : gates) {
			const uint8_t* ttBytes = reinterpret_cast<const uint8_t*>(&gate.tt);
			buffer.insert(buffer.end(), ttBytes, ttBytes + sizeof(uint64_t));

			for (uint32_t j = 0; j < lutK; ++j) {
				uint64_t inId = (j < gate.inputs.size()) ? gate.inputs[j] : 0ULL;
				const uint8_t* inBytes = reinterpret_cast<const uint8_t*>(&inId);
				buffer.insert(buffer.end(), inBytes, inBytes + sizeof(uint64_t));
			}
		}

		return buffer;
	}

	std::string writeToTempFile(const std::string& path) const {
		std::vector<uint8_t> bin = buildBinary();
		std::ofstream out(path, std::ios::binary);
		out.write(reinterpret_cast<const char*>(bin.data()), bin.size());
		out.close();
		return path;
	}
};

struct TempFileRemover {
	std::string path;
	TempFileRemover(const std::string& p) : path(p) {}
	~TempFileRemover() {
		std::remove(path.c_str());
	}
};
