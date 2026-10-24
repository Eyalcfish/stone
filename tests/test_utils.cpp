#include "test_framework.h"
#include "utils.h"

TEST_CASE("Utils_CalcTT_1Input") {
	// 1-input buffer: tt = 0b10 (bit 0 = 0, bit 1 = 1)
	uint64_t tt_buf = 0b10;
	ASSERT_EQ(calcTT(0, tt_buf), false);
	ASSERT_EQ(calcTT(1, tt_buf), true);

	// 1-input inverter: tt = 0b01 (bit 0 = 1, bit 1 = 0)
	uint64_t tt_not = 0b01;
	ASSERT_EQ(calcTT(0, tt_not), true);
	ASSERT_EQ(calcTT(1, tt_not), false);

	// Constant 0: tt = 0b00
	uint64_t tt_zero = 0b00;
	ASSERT_EQ(calcTT(0, tt_zero), false);
	ASSERT_EQ(calcTT(1, tt_zero), false);

	// Constant 1: tt = 0b11
	uint64_t tt_one = 0b11;
	ASSERT_EQ(calcTT(0, tt_one), true);
	ASSERT_EQ(calcTT(1, tt_one), true);
}

TEST_CASE("Utils_CalcTT_2Input_StandardGates") {
	// 2-input truth tables: input = (in1 << 1) | in0
	// in0 in1 -> index: (0,0)->0, (1,0)->1, (0,1)->2, (1,1)->3

	// AND: only (1,1)->1 => bit 3 is 1 => 0b1000 = 8
	uint64_t tt_and = 0x8;
	ASSERT_EQ(calcTT(0b00, tt_and), false);
	ASSERT_EQ(calcTT(0b01, tt_and), false);
	ASSERT_EQ(calcTT(0b10, tt_and), false);
	ASSERT_EQ(calcTT(0b11, tt_and), true);

	// OR: (0,0)->0, others 1 => bits 1,2,3 are 1 => 0b1110 = 0xE
	uint64_t tt_or = 0xE;
	ASSERT_EQ(calcTT(0b00, tt_or), false);
	ASSERT_EQ(calcTT(0b01, tt_or), true);
	ASSERT_EQ(calcTT(0b10, tt_or), true);
	ASSERT_EQ(calcTT(0b11, tt_or), true);

	// XOR: (0,1)->1, (1,0)->1 => bits 1,2 are 1 => 0b0110 = 0x6
	uint64_t tt_xor = 0x6;
	ASSERT_EQ(calcTT(0b00, tt_xor), false);
	ASSERT_EQ(calcTT(0b01, tt_xor), true);
	ASSERT_EQ(calcTT(0b10, tt_xor), true);
	ASSERT_EQ(calcTT(0b11, tt_xor), false);

	// NAND: ~(AND) => bits 0,1,2 are 1 => 0b0111 = 0x7
	uint64_t tt_nand = 0x7;
	ASSERT_EQ(calcTT(0b00, tt_nand), true);
	ASSERT_EQ(calcTT(0b01, tt_nand), true);
	ASSERT_EQ(calcTT(0b10, tt_nand), true);
	ASSERT_EQ(calcTT(0b11, tt_nand), false);

	// NOR: ~(OR) => bit 0 is 1 => 0b0001 = 0x1
	uint64_t tt_nor = 0x1;
	ASSERT_EQ(calcTT(0b00, tt_nor), true);
	ASSERT_EQ(calcTT(0b01, tt_nor), false);
	ASSERT_EQ(calcTT(0b10, tt_nor), false);
	ASSERT_EQ(calcTT(0b11, tt_nor), false);

	// XNOR: ~(XOR) => bits 0,3 are 1 => 0b1001 = 0x9
	uint64_t tt_xnor = 0x9;
	ASSERT_EQ(calcTT(0b00, tt_xnor), true);
	ASSERT_EQ(calcTT(0b01, tt_xnor), false);
	ASSERT_EQ(calcTT(0b10, tt_xnor), false);
	ASSERT_EQ(calcTT(0b11, tt_xnor), true);
}

TEST_CASE("Utils_CalcTT_3Input_ComplexGates") {
	// 3-input truth table: indices 0..7
	// Majority gate: output 1 if at least two inputs are 1
	// Inputs with >= 2 ones: 3 (011), 5 (101), 6 (110), 7 (111)
	// tt = (1<<3) | (1<<5) | (1<<6) | (1<<7) = 8 + 32 + 64 + 128 = 232 = 0xE8
	uint64_t tt_maj3 = 0xE8;
	for (uint8_t in = 0; in < 8; ++in) {
		int ones = (in & 1) + ((in >> 1) & 1) + ((in >> 2) & 1);
		bool expected = (ones >= 2);
		ASSERT_EQ(calcTT(in, tt_maj3), expected);
	}

	// 3-input XOR (odd parity):
	// Inputs with odd ones: 1 (001), 2 (010), 4 (100), 7 (111)
	// tt = (1<<1) | (1<<2) | (1<<4) | (1<<7) = 2 + 4 + 16 + 128 = 150 = 0x96
	uint64_t tt_xor3 = 0x96;
	for (uint8_t in = 0; in < 8; ++in) {
		int ones = (in & 1) + ((in >> 1) & 1) + ((in >> 2) & 1);
		bool expected = (ones % 2 == 1);
		ASSERT_EQ(calcTT(in, tt_xor3), expected);
	}

	// 2-to-1 Multiplexer: in0 = A, in1 = B, in2 = S. Output = S ? B : A
	// index: bit0=A, bit1=B, bit2=S
	// S=0 (indices 0..3): output = A (indices 1, 3 have A=1) => bits 1, 3
	// S=1 (indices 4..7): output = B (indices 6, 7 have B=1) => bits 6, 7
	// tt = (1<<1) | (1<<3) | (1<<6) | (1<<7) = 2 + 8 + 64 + 128 = 202 = 0xCA
	uint64_t tt_mux = 0xCA;
	for (uint8_t in = 0; in < 8; ++in) {
		bool a = (in & 1) != 0;
		bool b = ((in >> 1) & 1) != 0;
		bool s = ((in >> 2) & 1) != 0;
		bool expected = s ? b : a;
		ASSERT_EQ(calcTT(in, tt_mux), expected);
	}
}

TEST_CASE("Utils_CalcTT_6Input_Full64Bit") {
	// 6-input LUT tests up to all 64 bits of uint64_t truth table
	uint64_t tt_pattern = 0xA5A5A5A55A5A5A5AULL;
	for (uint8_t in = 0; in < 64; ++in) {
		bool expected = (tt_pattern >> in) & 1ULL;
		ASSERT_EQ(calcTT(in, tt_pattern), expected);
	}

	// Test boundary bits: bit 0 and bit 63
	uint64_t tt_edge = (1ULL << 0) | (1ULL << 63);
	ASSERT_EQ(calcTT(0, tt_edge), true);
	ASSERT_EQ(calcTT(63, tt_edge), true);
	ASSERT_EQ(calcTT(1, tt_edge), false);
	ASSERT_EQ(calcTT(62, tt_edge), false);
}
