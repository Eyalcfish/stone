cmake_minimum_required(VERSION 3.16)
project(stone VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
	add_compile_options(/W4 /O2)
else()
	add_compile_options(-Wall -Wextra -Wno-ignored-attributes -O3)
endif()

# Core Library
add_library(stone_lib STATIC
	lib/stone.cpp
	lib/stone.h
	lib/utils.h
)
target_include_directories(stone_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# CLI Tool (WIP)
add_executable(stone_cli
	cli/main.cpp
)
target_link_libraries(stone_cli PRIVATE stone_lib)

# Tests
enable_testing()

add_executable(stone_tests
	tests/test_main.cpp
	tests/test_utils.cpp
	tests/test_stone.cpp
)
target_include_directories(stone_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(stone_tests PRIVATE stone_lib)

add_test(NAME stone_tests COMMAND stone_tests)

# Benchmark Tool
add_executable(stone_benchmark
	tests/benchmark.cpp
)
target_include_directories(stone_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(stone_benchmark PRIVATE stone_lib)
