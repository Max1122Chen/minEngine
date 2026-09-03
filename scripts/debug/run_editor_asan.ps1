# Run ASan-instrumented Editor with LLVM-MinGW ASan runtime on PATH.
#
# Usage (from repo root):
#   .\scripts\debug\run_editor_asan.ps1
#   .\scripts\debug\run_editor_asan.ps1 -Project "minEngine\MyMEProject\MyMEProject.meproject"
#   .\scripts\debug\run_editor_asan.ps1 -Rhi vulkan
#
# ASan report tips:
#   - Look for "==ERROR: AddressSanitizer: ..."
#   - Symbolized stacks need llvm-symbolizer on PATH (comes with LLVM-MinGW bin).
#   - ASAN_OPTIONS defaults below favor crash bugs over leak noise.

[CmdletBinding()]
param(
    [string]$LlvmMingwRoot = "D:\Dev\llvm-mingw\llvm-mingw-20260616-ucrt-x86_64",
    [string]$Project = "",
    [string]$Rhi = "opengl",
    [string]$ExtraArgs = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$BinAsan = Join-Path $RepoRoot "minEngine\bin-asan"
$Exe = Join-Path $BinAsan "Editor.exe"

if (-not (Test-Path $Exe)) {
    throw "Missing $Exe — run configure_asan.ps1 + build_asan.ps1 first."
}
if (-not (Test-Path (Join-Path $LlvmMingwRoot "bin\libclang_rt.asan_dynamic-x86_64.dll"))) {
    throw "ASan runtime DLL missing under $LlvmMingwRoot\bin"
}

if ([string]::IsNullOrWhiteSpace($Project)) {
    $candidates = @(
        (Join-Path $RepoRoot "minEngine\MyMEProject\MyMEProject.meproject"),
        (Join-Path $RepoRoot "minEngine-editor\minEngine\MyMEProject\MyMEProject.meproject")
    )
    $Project = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Project -or -not (Test-Path $Project)) {
    throw "Project .meproject not found. Pass -Project <path>."
}

$env:Path = "$(Join-Path $LlvmMingwRoot 'bin');$BinAsan;$env:Path"
if (-not $env:ASAN_OPTIONS) {
    $env:ASAN_OPTIONS = "halt_on_error=1:abort_on_error=1:detect_leaks=0:allocator_may_return_null=0"
}
if (-not $env:ASAN_SYMBOLIZER_PATH) {
    $symbolizer = Join-Path $LlvmMingwRoot "bin\llvm-symbolizer.exe"
    if (Test-Path $symbolizer) {
        $env:ASAN_SYMBOLIZER_PATH = $symbolizer
    }
}

# Runtime DLLs required by the clang++ ASan build (keep beside Editor.exe too).
$runtimeDlls = @(
    "libclang_rt.asan_dynamic-x86_64.dll",
    "libc++.dll",
    "libunwind.dll"
)
foreach ($dll in $runtimeDlls) {
    $src = Join-Path $LlvmMingwRoot "bin\$dll"
    $dst = Join-Path $BinAsan $dll
    if ((Test-Path $src) -and -not (Test-Path $dst)) {
        Copy-Item -Force $src $dst
    }
}
# Assimp is often a separate DLL used by the engine.
$assimpSrc = Join-Path $RepoRoot "minEngine\bin\libassimp-6.dll"
if ((Test-Path $assimpSrc) -and -not (Test-Path (Join-Path $BinAsan "libassimp-6.dll"))) {
    Copy-Item -Force $assimpSrc (Join-Path $BinAsan "libassimp-6.dll")
}

Write-Host "Exe     : $Exe"
Write-Host "Project : $Project"
Write-Host "RHI     : $Rhi"
Write-Host "ASAN_OPTIONS=$($env:ASAN_OPTIONS)"
Write-Host ""

$argList = @("--rhi", $Rhi, "--project", $Project)
if (-not [string]::IsNullOrWhiteSpace($ExtraArgs)) {
    $argList += $ExtraArgs.Split(" ", [System.StringSplitOptions]::RemoveEmptyEntries)
}

Push-Location $BinAsan
try {
    & $Exe @argList 2>&1 | Tee-Object -FilePath (Join-Path $BinAsan "asan_last_run.log")
    Write-Host "Editor exit code: $LASTEXITCODE"
}
finally {
    Pop-Location
}
