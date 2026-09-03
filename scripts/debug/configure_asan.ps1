# Configure a separate AddressSanitizer build tree (does not touch minEngine/build).
#
# Prerequisites:
#   - LLVM-MinGW (UCRT) with clang++ ASan runtime
#     Default: D:\Dev\llvm-mingw\llvm-mingw-20260616-ucrt-x86_64
#   - CMake (D:\Dev\CMake\bin\cmake.exe or on PATH)
#
# Usage (from repo root):
#   .\scripts\debug\configure_asan.ps1
#   .\scripts\debug\configure_asan.ps1 -LlvmMingwRoot "D:\path\to\llvm-mingw-*-ucrt-x86_64"
#
# Then build:
#   mingw32-make -C minEngine\build-asan Editor -j8
# Or:
#   .\scripts\debug\build_asan.ps1

[CmdletBinding()]
param(
    [string]$LlvmMingwRoot = "",
    [string]$BuildDir = "",
    [string]$CMakePath = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$SourceDir = Join-Path $RepoRoot "minEngine"

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $SourceDir "build-asan"
}
if ([string]::IsNullOrWhiteSpace($CMakePath)) {
    $candidates = @(
        "D:\Dev\CMake\bin\cmake.exe",
        (Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source)
    )
    $CMakePath = $candidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
}
if (-not $CMakePath -or -not (Test-Path $CMakePath)) {
    throw "cmake.exe not found. Pass -CMakePath or install CMake."
}

if ([string]::IsNullOrWhiteSpace($LlvmMingwRoot)) {
    $defaultRoot = "D:\Dev\llvm-mingw\llvm-mingw-20260616-ucrt-x86_64"
    if (Test-Path $defaultRoot) {
        $LlvmMingwRoot = $defaultRoot
    }
    else {
        $discovered = Get-ChildItem "D:\Dev\llvm-mingw" -Directory -ErrorAction SilentlyContinue |
            Where-Object { Test-Path (Join-Path $_.FullName "bin\clang++.exe") } |
            Select-Object -First 1 -ExpandProperty FullName
        if ($discovered) { $LlvmMingwRoot = $discovered }
    }
}
if (-not $LlvmMingwRoot -or -not (Test-Path (Join-Path $LlvmMingwRoot "bin\clang++.exe"))) {
    throw "LLVM-MinGW root not found (need bin\clang++.exe). Pass -LlvmMingwRoot."
}

$Clang = Join-Path $LlvmMingwRoot "bin\clang.exe"
$ClangXX = Join-Path $LlvmMingwRoot "bin\clang++.exe"
$Make = Join-Path $LlvmMingwRoot "bin\mingw32-make.exe"
if (-not (Test-Path $Make)) {
    $Make = (Get-Command mingw32-make -ErrorAction SilentlyContinue).Source
}

Write-Host "RepoRoot     : $RepoRoot"
Write-Host "SourceDir    : $SourceDir"
Write-Host "BuildDir     : $BuildDir"
Write-Host "LLVM-MinGW   : $LlvmMingwRoot"
Write-Host "CMake        : $CMakePath"
Write-Host "C compiler   : $Clang"
Write-Host "C++ compiler : $ClangXX"

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

# Prefer LLVM-MinGW bin first so clang finds its own asan runtime / linker.
$env:Path = "$(Join-Path $LlvmMingwRoot 'bin');$env:Path"

& $CMakePath -S $SourceDir -B $BuildDir -G "MinGW Makefiles" `
    -DCMAKE_BUILD_TYPE=Debug `
    -DCMAKE_C_COMPILER="$Clang" `
    -DCMAKE_CXX_COMPILER="$ClangXX" `
    -DMINENGINE_ENABLE_ASAN=ON `
    -DBUILD_EDITOR=ON `
    -DBUILD_TESTS=OFF `
    -DBUILD_PLAYGROUND=OFF

if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed (exit $LASTEXITCODE)."
}

Write-Host ""
Write-Host "Configure OK. Build with:"
Write-Host "  `$env:Path = '$LlvmMingwRoot\bin;' + `$env:Path"
if ($Make) {
    Write-Host "  & '$Make' -C '$BuildDir' Editor -j8"
}
else {
    Write-Host "  cmake --build '$BuildDir' --target Editor -j 8"
}
Write-Host "Or: .\scripts\debug\build_asan.ps1"
Write-Host "Run: .\scripts\debug\run_editor_asan.ps1"
