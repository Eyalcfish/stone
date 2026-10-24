#include "test_framework.h"
#include "test_helpers.h"
#include "stone.h"
#include "utils.h"
#include <vector>
#include <string>

TEST_CASE("Stone_StructSizes_And_HeaderValidation") {
	ASSERT_EQ(sizeof(StbHeader), 16);
	ASSERT_EQ(sizeof(Pebble), 8);
	ASSERT_EQ(sizeof(rock), 16);

	std::string path = "test_header.stb";
	TempFileRemover remover(path);

	StbBuilder builder(4, 2, 2);
	builder.addGate(0x6, {0, 1}); // XOR
	builder.addGate(0x8, {2, 3}); // AND
	builder.writeToTempFile(path);

	Stone stone(path);
	ASSERT_EQ(stone.stbHeader.amountOfInputs, 4);
	ASSERT_EQ(stone.stbHeader.amountOfOutputs, 2);
	ASSERT_EQ(stone.stbHeader.amountOfGates, 6);
	ASSERT_EQ(static_cast<int>(stone.stbHeader.LUTK), 2);
}

TEST_CASE("Stone_SingleGate_AND") {
	std::string path = "test_and.stb";
	TempFileRemover remover(path);

	StbBuilder builder(2, 1, 2);
	builder.addGate(0x8, {0, 1}); // AND
	builder.writeToTempFile(path);

	Stone stone(path);

	for (int a = 0; a <= 1; ++a) {
		for (int b = 0; b <= 1; ++b) {
			std::vector<rock> input = createInputRocks({a != 0, b != 0});
			std::vector<rock> output = stone.inference(input);
			std::vector<bool> bits = extractOutputBits(output, 1);

			bool expected = (a != 0) && (b != 0);
			ASSERT_EQ(bits[0], expected);
		}
	}
}

TEST_CASE("Stone_SingleGate_OR") {
	std::string path = "test_or.stb";
	TempFileRemover remover(path);

	StbBuilder builder(2, 1, 2);
	builder.addGate(0xE, {0, 1}); // OR
	builder.writeToTempFile(path);

	Stone stone(path);

	for (int a = 0; a <= 1; ++a) {
		for (int b = 0; b <= 1; ++b) {
			std::vector<rock> input = createInputRocks({a != 0, b != 0});
			std::vector<rock> output = stone.inference(input);
			std::vector<bool> bits = extractOutputBits(output, 1);

			bool expected = (a != 0) || (b != 0);
			ASSERT_EQ(bits[0], expected);
		}
	}
}

TEST_CASE("Stone_SingleGate_XOR") {
	std::string path = "test_xor.stb";
	TempFileRemover remover(path);

	StbBuilder builder(2, 1, 2);
	builder.addGate(0x6, {0, 1}); // XOR
	builder.writeToTempFile(path);

	Stone stone(path);

	for (int a = 0; a <= 1; ++a) {
		for (int b = 0; b <= 1; ++b) {
			std::vector<rock> input = createInputRocks({a != 0, b != 0});
			std::vector<rock> output = stone.inference(input);
			std::vector<bool> bits = extractOutputBits(output, 1);

			bool expected = (a != 0) ^ (b != 0);
			ASSERT_EQ(bits[0], expected);
		}
	}
}

TEST_CASE("Stone_SingleGate_NAND") {
	std::string path = "test_nand.stb";
	TempFileRemover remover(path);

	StbBuilder builder(2, 1, 2);
	builder.addGate(0x7, {0, 1}); // NAND
	builder.writeToTempFile(path);

	Stone stone(path);

	for (int a = 0; a <= 1; ++a) {
		for (int b = 0; b <= 1; ++b) {
			std::vector<rock> input = createInputRocks({a != 0, b != 0});
			std::vector<rock> output = stone.inference(input);
			std::vector<bool> bits = extractOutputBits(output, 1);

			bool expected = !((a != 0) && (b != 0));
			ASSERT_EQ(bits[0], expected);
		}
	}
}

TEST_CASE("Stone_MultiOutput_HalfAdder") {
	// Inputs: A (0), B (1)
	// Output 0: Sum = A ^ B (gate 0, id 2)
	// Output 1: Carry = A & B (gate 1, id 3)
	std::string path = "test_half_adder.stb";
	TempFileRemover remover(path);

	StbBuilder builder(2, 2, 2);
	builder.addGate(0x6, {0, 1}); // Gate 0: XOR(A, B) -> Sum
	builder.addGate(0x8, {0, 1}); // Gate 1: AND(A, B) -> Carry
	builder.writeToTempFile(path);

	Stone stone(path);

	for (int a = 0; a <= 1; ++a) {
		for (int b = 0; b <= 1; ++b) {
			std::vector<rock> input = createInputRocks({a != 0, b != 0});
			std::vector<rock> output = stone.inference(input);
			std::vector<bool> bits = extractOutputBits(output, 2);

			bool expectedSum = (a != 0) ^ (b != 0);
			bool expectedCarry = (a != 0) && (b != 0);

			ASSERT_EQ(bits[0], expectedSum);
			ASSERT_EQ(bits[1], expectedCarry);
		}
	}
}

TEST_CASE("Stone_MultiLayer_FullAdder") {
	// Inputs: A (0), B (1), Cin (2)
	// Gate 0 (id 3): P = XOR(A, B)
	// Gate 1 (id 4): G = AND(A, B)
	// Gate 2 (id 5): C1 = AND(P, Cin) -> AND(gate 0, input 2)
	// Gate 3 (id 6): Sum = XOR(P, Cin) -> XOR(gate 0, input 2) -> Output 0
	// Gate 4 (id 7): Cout = OR(G, C1) -> OR(gate 1, gate 2) -> Output 1
	std::string path = "test_full_adder.stb";
	TempFileRemover remover(path);

	StbBuilder builder(3, 2, 2);
	builder.addGate(0x6, {0, 1}); // Gate 0 (id 3): P = A ^ B
	builder.addGate(0x8, {0, 1}); // Gate 1 (id 4): G = A & B
	builder.addGate(0x8, {3, 2}); // Gate 2 (id 5): C1 = P & Cin
	builder.addGate(0x6, {3, 2}); // Gate 3 (id 6): Sum = P ^ Cin (Output 0)
	builder.addGate(0xE, {4, 5}); // Gate 4 (id 7): Cout = G | C1 (Output 1)
	builder.writeToTempFile(path);

	Stone stone(path);

	for (int a = 0; a <= 1; ++a) {
		for (int b = 0; b <= 1; ++b) {
			for (int cin = 0; cin <= 1; ++cin) {
				std::vector<rock> input = createInputRocks({a != 0, b != 0, cin != 0});
				std::vector<rock> output = stone.inference(input);
				std::vector<bool> bits = extractOutputBits(output, 2);

				int total = a + b + cin;
				bool expectedSum = (total % 2) != 0;
				bool expectedCout = (total >= 2);

				ASSERT_EQ(bits[0], expectedSum);
				ASSERT_EQ(bits[1], expectedCout);
			}
		}
	}
}

TEST_CASE("Stone_MultiLayer_ParityChain") {
	// Inputs: A (0), B (1), C (2), D (3)
	// Gate 0 (id 4): XOR(A, B)
	// Gate 1 (id 5): XOR(Gate 0, C) -> XOR(4, 2)
	// Gate 2 (id 6): XOR(Gate 1, D) -> XOR(5, 3) (Output 0)
	std::string path = "test_parity4.stb";
	TempFileRemover remover(path);

	StbBuilder builder(4, 1, 2);
	builder.addGate(0x6, {0, 1}); // Gate 0 (id 4)
	builder.addGate(0x6, {4, 2}); // Gate 1 (id 5)
	builder.addGate(0x6, {5, 3}); // Gate 2 (id 6) -> Output
	builder.writeToTempFile(path);

	Stone stone(path);

	for (int a = 0; a <= 1; ++a) {
		for (int b = 0; b <= 1; ++b) {
			for (int c = 0; c <= 1; ++c) {
				for (int d = 0; d <= 1; ++d) {
					std::vector<rock> input = createInputRocks({a != 0, b != 0, c != 0, d != 0});
					std::vector<rock> output = stone.inference(input);
					std::vector<bool> bits = extractOutputBits(output, 1);

					bool expected = ((a ^ b ^ c ^ d) != 0);
					ASSERT_EQ(bits[0], expected);
				}
			}
		}
	}
}

TEST_CASE("Stone_LUT3_Majority") {
	// LUTK = 3
	// 3 inputs (A, B, C), 1 output: Majority-3 gate (tt = 0xE8)
	std::string path = "test_lut3_maj.stb";
	TempFileRemover remover(path);

	StbBuilder builder(3, 1, 3);
	builder.addGate(0xE8, {0, 1, 2}); // Majority-3
	builder.writeToTempFile(path);

	Stone stone(path);
	ASSERT_EQ(static_cast<int>(stone.stbHeader.LUTK), 3);

	for (int a = 0; a <= 1; ++a) {
		for (int b = 0; b <= 1; ++b) {
			for (int c = 0; c <= 1; ++c) {
				std::vector<rock> input = createInputRocks({a != 0, b != 0, c != 0});
				std::vector<rock> output = stone.inference(input);
				std::vector<bool> bits = extractOutputBits(output, 1);

				bool expected = (a + b + c >= 2);
				ASSERT_EQ(bits[0], expected);
			}
		}
	}
}

TEST_CASE("Stone_LUT4_Parity") {
	// LUTK = 4
	// 4 inputs (0, 1, 2, 3), 1 output: 4-input parity
	// Parity tt: bit is 1 when odd number of bits in index
	uint64_t tt_parity4 = 0;
	for (uint64_t i = 0; i < 16; ++i) {
		int ones = (i & 1) + ((i >> 1) & 1) + ((i >> 2) & 1) + ((i >> 3) & 1);
		if (ones % 2 == 1) {
			tt_parity4 |= (1ULL << i);
		}
	}

	std::string path = "test_lut4_parity.stb";
	TempFileRemover remover(path);

	StbBuilder builder(4, 1, 4);
	builder.addGate(tt_parity4, {0, 1, 2, 3});
	builder.writeToTempFile(path);

	Stone stone(path);
	ASSERT_EQ(static_cast<int>(stone.stbHeader.LUTK), 4);

	for (int i = 0; i < 16; ++i) {
		bool b0 = (i & 1) != 0;
		bool b1 = ((i >> 1) & 1) != 0;
		bool b2 = ((i >> 2) & 1) != 0;
		bool b3 = ((i >> 3) & 1) != 0;

		std::vector<rock> input = createInputRocks({b0, b1, b2, b3});
		std::vector<rock> output = stone.inference(input);
		std::vector<bool> bits = extractOutputBits(output, 1);

		bool expected = (b0 ^ b1 ^ b2 ^ b3);
		ASSERT_EQ(bits[0], expected);
	}
}

TEST_CASE("Stone_MultiRock_WideCircuitInputs") {
	// 130 inputs: spans across Rock 0 (bits 0..127) and Rock 1 (bits 128..129)
	// 2 outputs:
	// Output 0: XOR of input 0 (in rock 0) and input 129 (in rock 1)
	// Output 1: AND of input 63 (lane 0) and input 64 (lane 1)
	std::string path = "test_wide_inputs.stb";
	TempFileRemover remover(path);

	StbBuilder builder(130, 2, 2);
	builder.addGate(0x6, {0, 129}); // Gate 0: XOR(in0, in129) -> Output 0
	builder.addGate(0x8, {63, 64});  // Gate 1: AND(in63, in64) -> Output 1
	builder.writeToTempFile(path);

	Stone stone(path);
	ASSERT_EQ(stone.stbHeader.amountOfInputs, 130);
	ASSERT_EQ(stone.stbHeader.amountOfOutputs, 2);

	std::vector<bool> inputBits(130, false);
	// Test case 1: all false
	{
		std::vector<rock> input = createInputRocks(inputBits);
		std::vector<rock> output = stone.inference(input);
		std::vector<bool> bits = extractOutputBits(output, 2);
		ASSERT_FALSE(bits[0]);
		ASSERT_FALSE(bits[1]);
	}

	// Test case 2: in0 = true, in129 = false => XOR = true
	{
		inputBits[0] = true;
		inputBits[129] = false;
		std::vector<rock> input = createInputRocks(inputBits);
		std::vector<rock> output = stone.inference(input);
		std::vector<bool> bits = extractOutputBits(output, 2);
		ASSERT_TRUE(bits[0]);
		ASSERT_FALSE(bits[1]);
	}

	// Test case 3: in0 = true, in129 = true => XOR = false
	{
		inputBits[0] = true;
		inputBits[129] = true;
		std::vector<rock> input = createInputRocks(inputBits);
		std::vector<rock> output = stone.inference(input);
		std::vector<bool> bits = extractOutputBits(output, 2);
		ASSERT_FALSE(bits[0]);
		ASSERT_FALSE(bits[1]);
	}

	// Test case 4: in63 = true, in64 = true => AND = true
	{
		inputBits[63] = true;
		inputBits[64] = true;
		std::vector<rock> input = createInputRocks(inputBits);
		std::vector<rock> output = stone.inference(input);
		std::vector<bool> bits = extractOutputBits(output, 2);
		ASSERT_TRUE(bits[1]);
	}
}

TEST_CASE("Stone_MultiRock_WideCircuitOutputs") {
	// Circuit with 2 inputs, 130 outputs
	// All outputs are copies or inversions of input 0 or input 1
	std::string path = "test_wide_outputs.stb";
	TempFileRemover remover(path);

	StbBuilder builder(2, 130, 2);
	for (uint32_t i = 0; i < 130; ++i) {
		if (i % 2 == 0) {
			// XOR with input 1: if input 1 is 0, outputs input 0
			builder.addGate(0x6, {0, 1});
		} else {
			// AND of input 0 and input 1
			builder.addGate(0x8, {0, 1});
		}
	}
	builder.writeToTempFile(path);

	Stone stone(path);
	ASSERT_EQ(stone.stbHeader.amountOfOutputs, 130);

	// Test with in0 = true, in1 = false
	{
		std::vector<rock> input = createInputRocks({true, false});
		std::vector<rock> output = stone.inference(input);
		ASSERT_EQ(output.size(), 2); // 130 bits requires 2 rocks (2 * 128 = 256 bits)
		std::vector<bool> bits = extractOutputBits(output, 130);

		for (uint32_t i = 0; i < 130; ++i) {
			if (i % 2 == 0) {
				ASSERT_TRUE(bits[i]); // true ^ false = true
			} else {
				ASSERT_FALSE(bits[i]); // true & false = false
			}
		}
	}

	// Test with in0 = true, in1 = true
	{
		std::vector<rock> input = createInputRocks({true, true});
		std::vector<rock> output = stone.inference(input);
		std::vector<bool> bits = extractOutputBits(output, 130);

		for (uint32_t i = 0; i < 130; ++i) {
			if (i % 2 == 0) {
				ASSERT_FALSE(bits[i]); // true ^ true = false
			} else {
				ASSERT_TRUE(bits[i]); // true & true = true
			}
		}
	}
}


TEST_CASE("Stone_Caching_SharedSubexpression") {
	// Diamond DAG with shared subexpression:
	// in0, in1
	// Gate 0 (id 2): XOR(in0, in1)
	// Gate 1 (id 3): AND(Gate 0, Gate 0) -> Both inputs share Gate 0
	// Gate 2 (id 4): OR(Gate 1, Gate 0) -> Shares Gate 1 and Gate 0
	std::string path = "test_caching_shared.stb";
	TempFileRemover remover(path);

	StbBuilder builder(2, 1, 2);
	builder.addGate(0x6, {0, 1}); // Gate 0: in0 ^ in1
	builder.addGate(0x8, {2, 2}); // Gate 1: Gate 0 & Gate 0
	builder.addGate(0xE, {3, 2}); // Gate 2: Gate 1 | Gate 0 (Output 0)
	builder.writeToTempFile(path);

	Stone stone(path);

	for (int a = 0; a <= 1; ++a) {
		for (int b = 0; b <= 1; ++b) {
			std::vector<rock> input = createInputRocks({a != 0, b != 0});
			std::vector<rock> output = stone.inference(input);
			std::vector<bool> bits = extractOutputBits(output, 1);

			bool g0 = (a != 0) ^ (b != 0);
			bool g1 = g0 && g0;
			bool g2 = g1 || g0;

			ASSERT_EQ(bits[0], g2);
		}
	}
}

TEST_CASE("Stone_Caching_ResetBetweenInferences") {
	// Ensure that cache state is completely zeroed out between subsequent inference() calls
	std::string path = "test_caching_reset.stb";
	TempFileRemover remover(path);

	StbBuilder builder(2, 2, 2);
	builder.addGate(0x6, {0, 1}); // XOR (Sum)
	builder.addGate(0x8, {0, 1}); // AND (Carry)
	builder.writeToTempFile(path);

	Stone stone(path);

	// Alternate inputs 50 times
	for (int iter = 0; iter < 50; ++iter) {
		// Input 1: (true, true) => Sum=0, Carry=1
		{
			std::vector<rock> in1 = createInputRocks({true, true});
			std::vector<rock> out1 = stone.inference(in1);
			std::vector<bool> b1 = extractOutputBits(out1, 2);
			ASSERT_FALSE(b1[0]);
			ASSERT_TRUE(b1[1]);
		}
		// Input 2: (true, false) => Sum=1, Carry=0
		{
			std::vector<rock> in2 = createInputRocks({true, false});
			std::vector<rock> out2 = stone.inference(in2);
			std::vector<bool> b2 = extractOutputBits(out2, 2);
			ASSERT_TRUE(b2[0]);
			ASSERT_FALSE(b2[1]);
		}
		// Input 3: (false, false) => Sum=0, Carry=0
		{
			std::vector<rock> in3 = createInputRocks({false, false});
			std::vector<rock> out3 = stone.inference(in3);
			std::vector<bool> b3 = extractOutputBits(out3, 2);
			ASSERT_FALSE(b3[0]);
			ASSERT_FALSE(b3[1]);
		}
	}
}
