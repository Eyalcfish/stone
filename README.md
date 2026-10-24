# Stone

Stone is a high-performance C++ library and toolchain designed for simulating, evaluating, and translating hardware Look-Up Table (LUT) boolean logic networks using SIMD-accelerated bit manipulation.

---

## Current Status & Roadmap

| Component | Status | Description |
| :--- | :--- | :--- |
| **Binary `.stb` Loader** | Implemented | Binary serialization parser and memory slab reconstruction |
| **Inference Engine** | Implemented | High-throughput recursive truth-table evaluation over SIMD vector types |
| **Memoization / Caching** | Implemented | Intra-inference gate output caching; eliminates redundant DAG evaluations |
| **Bitwise Utilities** | Implemented | Cross-lane bit addressing (`setValue`, `getValue`, `calcTT`) |
| **Test Suite** | Implemented | 22 unit & integration test suites (>1,400 assertions passing) |
| **Performance Benchmark** | Implemented | Benchmark tool comparing cached vs non-cached throughput & latency |
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
│   ├── stone.cpp                    # Core implementation: .stb parser, caching, & inference engine
│   └── utils.h                      # Bit manipulation primitives and truth-table evaluation
├── tests/
│   ├── test_framework.h             # Lightweight, zero-dependency C++ test runner & assertions
│   ├── test_helpers.h               # StbBuilder utility for programmatically generating test circuits
│   ├── test_utils.cpp               # Unit tests for utils.h (calcTT, setValue, getValue)
│   ├── test_stone.cpp               # Integration tests for Stone loading, logic gates, adders, & caching
│   ├── test_main.cpp                # Test runner entrypoint
│   └── benchmark.cpp                # Performance benchmark (caching speedup, throughput, scaling)
├── CMakeLists.txt                   # CMake build system (targets: stone_lib, stone_cli, stone_tests, stone_benchmark)
├── .gitignore                       # Build output and temporary file exclusions
└── README.md                        # Project documentation and architectural overview
```

### Detailed File Descriptions

#### `lib/` (Core Engine)
- **[`lib/stone.h`](lib/stone.h)**:
  - Defines vector SIMD types (`rock`, `uint128_t` backed by `__m128i`, configurable up to 512 bits).
  - Defines `StbHeader`: 16-byte packed metadata header storing `amountOfInputs`, `amountOfOutputs`, `amountOfGates`, and `LUTK`.
  - Defines `Pebble`: Represents a gate node in the circuit graph with a 64-bit truth table (`tt`) and flexible array of input pointers (`inputs[]`).
  - Declares the `Stone` class interface: binary loading, inference, internal memoization cache, and Verilog translation methods.
- **[`lib/stone.cpp`](lib/stone.cpp)**:
  - Implements the `.stb` binary file parser, allocating contiguous memory slabs for inputs and hidden LUT gates.
  - Resolves gate input IDs into direct memory pointers.
  - Implements memoized recursive LUT traversal (`Stone::pebbleProcess`) and vectorized inference (`Stone::inference`).
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

#### `tests/` (Test Suite & Benchmarking)
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
  - Tests memoization caching on shared subexpressions and cache reset between inference passes.
- **[`tests/test_main.cpp`](tests/test_main.cpp)**:
  - Main runner executing all registered test cases and returning standard exit codes.
- **[`tests/benchmark.cpp`](tests/benchmark.cpp)**:
  - High-precision benchmarking suite comparing cached vs non-cached latency, scaling on exponential DAGs, adders, and random logic networks.

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

### 3. Memoization / Caching
During inference, gates that are shared across multiple fanouts or outputs are memoized in `Stone::cache`:
- Gate index $i$ uses bit $2i$ as the `is_cached` flag and bit $2i+1$ as the `cached_value`.
- Redundant sub-tree re-evaluations are eliminated, converting exponential tree recursions $O(2^D)$ into linear DAG evaluations $O(\text{Gates})$.
- The cache buffer is zeroed out at the end of each `inference()` call via `memset`.

---

## Performance & Benchmarks

Benchmarked on an **AMD Ryzen 9 PRO 5945 (12-Core)**:

### 1. Exponential Tree / Shared Sub-expressions (Diamond DAG)
| Depth | Calls (No Cache) | No Cache Latency | Cached Latency | Speedup |
| :---: | :---: | :---: | :---: | :---: |
| **5** | 32 | 131 ns | 98 ns | **1.3x** |
| **10** | 1,024 | 4.36 µs | 137 ns | **31.9x** |
| **15** | 32,768 | 104 µs | 196 ns | **533x** |
| **20** | 1,048,576 | 3.44 ms | 249 ns | **13,846x** |
| **24** | 16,777,216 | 50.4 ms | 238 ns | **211,688x** |
| **28** | 268,435,456 | ~402 ms | 305 ns | **>1,300,000x** |

### 2. Multi-Output Circuit (Ripple-Carry Adder)
| Bits | Gates | No Cache Latency | Cached Latency | Speedup | Throughput |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **8-bit** | 46 | 502 ns | 456 ns | **1.10x** | 2.19 M inf/sec |
| **16-bit** | 94 | 1.78 µs | 770 ns | **2.32x** | 1.30 M inf/sec |
| **32-bit** | 190 | 6.89 µs | 1.64 µs | **4.19x** | 608 k inf/sec |
| **64-bit** | 382 | 24.48 µs | 3.13 µs | **7.82x** | 319 k inf/sec |

### 3. Throughput on Large-Scale Logic Networks
| Gates | Primary I/O | Inference Latency | Inferences / sec | Gate Evals / sec |
| :---: | :---: | :---: | :---: | :---: |
| **1,000** | 64 / 32 | 4.26 µs | 234,780 inf/s | **234.8 M gates/s** |
| **5,000** | 64 / 32 | 8.71 µs | 114,804 inf/s | **574.0 M gates/s** |
| **10,000** | 64 / 32 | 12.10 µs | 82,628 inf/s | **826.3 M gates/s** |

---

## Building & Testing

### Using CMake

1. **Configure and build with optimizations**:
   ```bash
   cmake -B build -G "Ninja"
   cmake --build build
   ```

2. **Run tests via CTest**:
   ```bash
   ctest --test-dir build --output-on-failure
   ```

3. **Run the performance benchmark**:
   ```bash
   ./build/stone_benchmark.exe
   ```

### Standalone Compilation (Without CMake)

```bash
# Build and run tests
g++ -O3 -std=c++17 -Ilib -Itests -Wno-ignored-attributes tests/test_main.cpp tests/test_utils.cpp tests/test_stone.cpp lib/stone.cpp -o stone_tests.exe
./stone_tests.exe

# Build and run benchmark
g++ -O3 -std=c++17 -Ilib -Itests -Wno-ignored-attributes tests/benchmark.cpp lib/stone.cpp -o stone_benchmark.exe
./stone_benchmark.exe
```
