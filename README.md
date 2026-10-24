# Stone

Stone is a high-performance C++ library and toolchain designed for simulating, evaluating, and translating hardware Look-Up Table (LUT) boolean logic networks using SIMD-accelerated bit manipulation.

---

## Current Status & Roadmap

| Component | Status | Description |
| :--- | :--- | :--- |
| **Binary `.stb` Loader** | Implemented | Binary serialization parser and memory slab reconstruction |
| **Inference Engine** | Implemented | High-throughput recursive truth-table evaluation over SIMD vector types |
| **Bitwise Utilities** | Implemented | Cross-lane bit addressing (`setValue`, `getValue`, `calcTT`) |
| **Test Suite** | Implemented | 20 unit & integration test suites (>1,100 assertions passing) |
| **CLI Tool** | **Not Implemented** | Command-line driver (`cli/main.cpp`) is currently a work-in-progress stub |
| **Verilog Translation** | **Not Implemented** | `Stone::to_verilog()` and `Stone::emit_verilog()` are planned for a future release |

---

## File Structure

The workspace is organized into clear functional modules:

```text
stone/
├── cli/
│   ├── src/                         # Additional CLI command modules (WIP)
│   └── main.cpp                     # CLI application entrypoint (WIP stub)
├── lib/
│   ├── stone.h                      # Public library header (StbHeader, Pebble, Stone class)
│   ├── stone.cpp                    # Core implementation: .stb parser & inference engine
│   └── utils.h                      # Bit manipulation primitives and truth-table evaluation
├── tests/
│   ├── test_framework.h             # Lightweight, zero-dependency C++ test runner & assertions
│   ├── test_helpers.h               # StbBuilder utility for programmatically generating test circuits
│   ├── test_utils.cpp               # Unit tests for utils.h (calcTT, setValue, getValue)
│   ├── test_stone.cpp               # Integration tests for Stone loading, logic gates, and adders
│   └── test_main.cpp                # Test runner entrypoint
├── CMakeLists.txt                   # CMake build system (targets: stone_lib, stone_cli, stone_tests)
├── .gitignore                       # Build output and temporary file exclusions
└── README.md                        # Project documentation and architectural overview
```

### Detailed File Descriptions

#### `lib/` (Core Engine)
- **[`lib/stone.h`](lib/stone.h)**:
  - Defines vector SIMD types (`rock`, `uint128_t` backed by `__m128i`, configurable up to 512 bits).
  - Defines `StbHeader`: 16-byte packed metadata header storing `amountOfInputs`, `amountOfOutputs`, `amountOfGates`, and `LUTK`.
  - Defines `Pebble`: Represents a gate node in the circuit graph with a 64-bit truth table (`tt`) and flexible array of input pointers (`inputs[]`).
  - Declares the `Stone` class interface: binary loading, inference, and Verilog translation methods.
- **[`lib/stone.cpp`](lib/stone.cpp)**:
  - Implements the `.stb` binary file parser, allocating contiguous memory slabs for inputs and hidden LUT gates.
  - Resolves gate input IDs into direct memory pointers.
  - Implements `Stone::pebbleProcess` (recursive LUT traversal) and `Stone::inference` (batch evaluation on SIMD packed vectors).
- **[`lib/utils.h`](lib/utils.h)**:
  - `calcTT(input, tt)`: Performs $K$-LUT truth-table lookups for up to 6-input gates ($2^6 = 64$ bits).
  - `setValue(Value, index, value)`: Branchless setting of arbitrary bit positions across 64-bit lanes and 128-bit `rock` boundaries.
  - `getValue(Value, index)`: Extracts single-bit state from packed `rock` vectors.

#### `cli/` (Command-Line Tool)
- **[`cli/main.cpp`](cli/main.cpp)**:
  - Entry point for the planned `stone` command-line executable.
  - *Current Status*: Stub implementation notifying users that CLI operations are under active development.
- **[`cli/src/`](cli/src/)**:
  - Reserved directory for CLI command parsers, options handling, and file format converters.

#### `tests/` (Test Suite)
- **[`tests/test_framework.h`](tests/test_framework.h)**:
  - Zero-dependency, header-only testing framework providing automatic test registration (`TEST_CASE`), assertion macros (`ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`), microsecond timing, and structured pass/fail summaries.
- **[`tests/test_helpers.h`](tests/test_helpers.h)**:
  - Contains `StbBuilder`: a fluent helper class to construct valid `.stb` binary files in memory and disk for testing.
  - Provides `createInputRocks` and `extractOutputBits` for bidirectional conversion between `std::vector<bool>` and `std::vector<rock>`.
  - Provides `TempFileRemover` RAII helper for cleaning up temporary test files.
- **[`tests/test_utils.cpp`](tests/test_utils.cpp)**:
  - Unit tests for 1-input (BUF, NOT), 2-input (AND, OR, XOR, NAND, NOR, XNOR), 3-input (Majority-3, Parity-3, 2-to-1 MUX), and 6-input (full 64-bit uint64_t masks) truth tables.
  - Thorough testing of `setValue` and `getValue` across 64-bit lane boundaries and 128-bit `rock` boundaries (up to 384 bits).
- **[`tests/test_stone.cpp`](tests/test_stone.cpp)**:
  - Validates `StbHeader`, `Pebble`, and `rock` binary sizes and packing.
  - Tests end-to-end circuit inference on standard gates (AND, OR, XOR, NAND).
  - Tests multi-output circuits (Half-Adder: Sum & Carry).
  - Tests multi-level circuits (Full-Adder with 5 interconnected gates; 4-input cascaded parity tree).
  - Tests higher-order LUT configurations: 3-LUT (Majority-3) and 4-LUT (16-bit truth table).
  - Tests wide circuits exceeding 128 inputs/outputs across multiple `rock` instances.
- **[`tests/test_main.cpp`](tests/test_main.cpp)**:
  - Main runner executing all registered test cases and returning standard exit codes.

---

## Architecture & Concepts

### 1. Rock (SIMD Vector Chunk)
The circuit bit states are packed into SIMD registers known as **rocks**. By default, a rock represents 128 bits (`__m128i`). The bit-manipulation primitives in `utils.h` allow seamless indexing across arbitrarily large bit vectors consisting of multiple rocks:
- Lane 0: bits `0 .. 63`
- Lane 1: bits `64 .. 127`
- Subsequent rocks: bits `128 .. 255`, `256 .. 383`, etc.

### 2. Pebble (LUT Graph Node)
Each logic node in the circuit graph is represented as a **Pebble**:
```cpp
typedef struct Pebble {
    uint64_t tt;        // 64-bit truth table
    Pebble* inputs[];   // Flexible array of pointers to preceding pebbles
} Pebble;
```
- **Inputs**: Stored in the input slab with minimal size (8 bytes per input).
- **Hidden Gates**: Each gate has size `(LUTK + 1) * sizeof(uint64_t)`. It holds a 64-bit truth table and $K$ input pointers.
- **Outputs**: The final `amountOfOutputs` gates in the hidden slab serve as the primary outputs of the circuit.

### 3. Stone Binary (`.stb`) Format
The `.stb` file is a packed binary format:
```
+-------------------------------------------------------------+
| StbHeader (16 bytes)                                        |
|   uint32_t amountOfInputs                                   |
|   uint32_t amountOfOutputs                                  |
|   uint32_t amountOfGates                                    |
|   uint8_t  LUTK                                             |
|   uint8_t  _padding[3]                                      |
+-------------------------------------------------------------+
| Input Pebbles (amountOfInputs * 8 bytes)                    |
|   [dummy / index uint64_t] x amountOfInputs                 |
+-------------------------------------------------------------+
| Hidden Gate Pebbles ((amountOfGates - inputs) * (K+1) * 8)  |
|   For each gate:                                            |
|     uint64_t tt (truth table)                               |
|     uint64_t input_id[0 .. K-1]                             |
+-------------------------------------------------------------+
```

---

## Building & Testing

### Prerequisites
- C++17 compatible compiler (e.g., GCC 9+, Clang 10+, or MSVC 2019+)
- CMake 3.16+ (optional, or compile directly)
- Build tool such as Ninja or Make

### Using CMake

1. **Configure the project**:
   ```bash
   cmake -B build -G "Ninja"
   ```

2. **Build library, CLI, and tests**:
   ```bash
   cmake --build build
   ```

3. **Run tests via CTest**:
   ```bash
   ctest --test-dir build --output-on-failure
   ```

4. **Run the test runner directly**:
   ```bash
   ./build/stone_tests.exe
   ```

### Standalone Compilation (Without CMake)

You can compile and run the test suite directly with `g++`:

```bash
g++ -std=c++17 -Ilib -Itests -Wno-ignored-attributes tests/test_main.cpp tests/test_utils.cpp tests/test_stone.cpp lib/stone.cpp -o stone_tests.exe
./stone_tests.exe
```
