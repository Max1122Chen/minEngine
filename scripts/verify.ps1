# Local verify: build Editor and run unified test smoke suite.
# Usage (from repo root): .\scripts\verify.ps1

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $RepoRoot "minEngine\build"
$BinDir = Join-Path $RepoRoot "minEngine\bin"

Write-Host "verify: cmake --build (Editor) ..."
cmake --build $BuildDir --target Editor
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Push-Location $BinDir
try {
    Write-Host "verify: Editor.exe test smoke ..."
    & ".\Editor.exe" test smoke
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
