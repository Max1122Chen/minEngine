# Build Editor with AddressSanitizer (requires configure_asan.ps1 first).
#
# Usage (from repo root):
#   .\scripts\debug\build_asan.ps1
#   .\scripts\debug\build_asan.ps1 -Jobs 4

[CmdletBinding()]
param(
    [string]$LlvmMingwRoot = "D:\Dev\llvm-mingw\llvm-mingw-20260616-ucrt-x86_64",
    [string]$BuildDir = "",
    [int]$Jobs = 8
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$SourceDir = Join-Path $RepoRoot "minEngine"
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $SourceDir "build-asan"
}

if (-not (Test-Path (Join-Path $BuildDir "Makefile"))) {
    throw "ASan build dir not configured: $BuildDir`nRun .\scripts\debug\configure_asan.ps1 first."
}
if (-not (Test-Path (Join-Path $LlvmMingwRoot "bin\clang++.exe"))) {
    throw "LLVM-MinGW not found at $LlvmMingwRoot"
}

$Make = Join-Path $LlvmMingwRoot "bin\mingw32-make.exe"
$env:Path = "$(Join-Path $LlvmMingwRoot 'bin');$env:Path"

# Avoid DLL lock from leftover Editor processes.
$killScript = Join-Path $PSScriptRoot "kill_editor_processes.ps1"
if (Test-Path $killScript) {
    & $killScript
}

Write-Host "Building Editor (ASan) in $BuildDir ..."
& $Make -C $BuildDir Editor "-j$Jobs"
if ($LASTEXITCODE -ne 0) {
    throw "ASan build failed (exit $LASTEXITCODE)."
}

$exe = Join-Path $SourceDir "bin-asan\Editor.exe"
if (Test-Path $exe) {
    Write-Host "Built: $exe"
}
else {
    Write-Host "Build finished; check $BuildDir for output location."
}
Write-Host "Run: .\scripts\debug\run_editor_asan.ps1"
