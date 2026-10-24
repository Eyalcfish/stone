# Stone - Automated Build, Test, and Benchmark Script
[CmdletBinding()]
param (
    [switch]$SkipBenchmark,
    [switch]$TestsOnly,
    [switch]$BenchmarkOnly
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "         STONE: BUILD, TEST & BENCHMARK RUNNER            " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$WorkspaceRoot = $PSScriptRoot
Set-Location $WorkspaceRoot

# Step 1: Configure and Build
Write-Host "`n[*] Configuring and building with CMake (Ninja / -O3)..." -ForegroundColor Yellow
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "cmake not found in PATH."
}

cmake -B build -G "Ninja"
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed."
}

cmake --build build --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed."
}

Write-Host "[+] Build completed successfully." -ForegroundColor Green

# Step 2: Run Unit & Integration Tests
if (-not $BenchmarkOnly) {
    Write-Host "`n[*] Running Test Suite..." -ForegroundColor Yellow
    & "$WorkspaceRoot\build\stone_tests.exe"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`n[-] Test suite reported failures." -ForegroundColor Red
        exit $LASTEXITCODE
    }
    Write-Host "[+] All tests passed!" -ForegroundColor Green
}

# Step 3: Run Performance Benchmark
if (-not $TestsOnly -and -not $SkipBenchmark) {
    Write-Host "`n[*] Running Performance Benchmark..." -ForegroundColor Yellow
    & "$WorkspaceRoot\build\stone_benchmark.exe"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`n[-] Benchmark exited with an error." -ForegroundColor Red
        exit $LASTEXITCODE
    }
    Write-Host "`n[+] Verification and benchmarking finished successfully!" -ForegroundColor Green
}

Write-Host "`n==========================================================" -ForegroundColor Cyan
Write-Host "                     ALL DONE!                            " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
