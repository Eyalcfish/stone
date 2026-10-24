@echo off
setlocal enabledelayedexpansion

echo ==========================================================
echo          STONE: BUILD, TEST ^& BENCHMARK RUNNER
echo ==========================================================

cd /d "%~dp0"

echo.
echo [*] Configuring and building with CMake (Ninja / -O3)...
cmake -B build -G "Ninja"
if %ERRORLEVEL% neq 0 (
    echo [-] CMake configuration failed.
    exit /b %ERRORLEVEL%
)

cmake --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo [-] Build failed.
    exit /b %ERRORLEVEL%
)
echo [+] Build completed successfully.

echo.
echo [*] Running Test Suite...
build\stone_tests.exe
if %ERRORLEVEL% neq 0 (
    echo [-] Test suite reported failures.
    exit /b %ERRORLEVEL%
)
echo [+] All tests passed!

echo.
echo [*] Running Performance Benchmark...
build\stone_benchmark.exe
if %ERRORLEVEL% neq 0 (
    echo [-] Benchmark exited with an error.
    exit /b %ERRORLEVEL%
)

echo.
echo ==========================================================
echo                      ALL DONE!
echo ==========================================================
