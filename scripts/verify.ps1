# Local verify: build minEngineTests and run unified test smoke suite.
# Usage (from repo root): .\scripts\verify.ps1

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $RepoRoot "minEngine\build"
$BinDir = Join-Path $RepoRoot "minEngine\bin"

Write-Host "verify: cmake --build (minEngineTests) ..."
cmake --build $BuildDir --target minEngineTests Editor
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Push-Location $BinDir
try {
    Write-Host "verify: minEngineTests.exe test smoke ..."
    & ".\minEngineTests.exe" test smoke
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
