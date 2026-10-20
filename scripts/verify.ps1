# Local verify: build minEngineTests and run unified test smoke suite.
# Usage (from repo root): .\scripts\verify.ps1

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $RepoRoot "minEngine\build"
$BinDir = Join-Path $RepoRoot "minEngine\bin"

# SampleEditor is built here because BUILD-F04 S01 asserts the state of BOTH editor hosts: an
# editor exe that is older than the editor DLL is a stale artifact and must not pass silently.
Write-Host "verify: cmake --build (minEngineTests, Editor, SampleEditor) ..."
cmake --build $BuildDir --target minEngineTests Editor SampleEditor
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# BUILD-F02 / O1+O2: the engine DLL must stay far away from the 65535 PE ordinal limit.
# BUILD-F03 C2 / BUILD-F04 S04: the engine export table must contain NO imgui entry. imgui is
# compiled by libMaximumEditord.dll now; an imgui symbol reappearing here means the family was
# moved back into the engine target (see the I-gimgui assert in minEngine/minEngine/CMakeLists.txt).
# BUILD-F03 C1 / BUILD-F04 S04: and NO glfw entry either. glfw is the shared glfw3.dll now, so an
# engine-defined glfw symbol means the static libglfw3.a crept back in - the same shape that gives
# the process two GLFW instances (the engine working on a copy the editor DLL never sees).
python (Join-Path $RepoRoot "scripts\audit_dll_exports.py") --max-exports 3000 --fail --forbid-buckets imgui,glfw
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# BUILD-F04 S04 / O3 in-repo half: every in-repo consumer of the post-S04 engine contract must
# still resolve. This one is never tolerated - it is the assertion that the 253 removed imgui
# entries were not a symbol some in-repo artifact actually needs from the engine.
python (Join-Path $RepoRoot "scripts\audit_dll_exports.py") --dll (Join-Path $BinDir "libminEngined.dll") --verify-coverage
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# BUILD-F04 S01/S02/S03: editor/module boundary. Every assertion starts with "does the artifact
# exist", so a missing libMaximumEditord.dll / editor exe fails loudly instead of skipping checks.
python (Join-Path $RepoRoot "scripts\audit_module_boundaries.py") --assert --fail
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# BUILD-F04 S02: both editor hosts must resolve every import from the editor DLL's export contract.
python (Join-Path $RepoRoot "scripts\audit_dll_exports.py") --dll (Join-Path $BinDir "libMaximumEditord.dll") --verify-coverage
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# BUILD-F03 C8/Q8: every engine import of the LIVE external product bins must be exported.
# The sibling minCraft* bins are where that contract is observable; the in-repo copies are stale.
$ExternalArgs = @(Get-ChildItem -Path (Join-Path (Split-Path -Parent $RepoRoot) "minCraft*") -Directory -ErrorAction SilentlyContinue |
    ForEach-Object { Join-Path $_.FullName "bin" } | Where-Object { Test-Path $_ } | ForEach-Object { "--consumer-dir"; $_ })
if ($ExternalArgs.Count -eq 0) { Write-Host "verify: no external product bins found, external contract NOT checked" }
else {
    # BUILD-F04 S04 transitional exception (design 4.1 + F03 C2): the LIVE external editor hosts
    # are pre-S04 artifacts - they still import the 253 imgui symbols the engine contract no longer
    # carries, which is what "stale" means after C2, not a regression. The exception is scoped to
    # the imgui BUCKET and to consumers outside this repo; it prints every tolerated name, fails on
    # any other missing symbol, and retires itself.
    # REMOVAL CONDITION: delete --transitional-missing-bucket below as soon as the external
    # minCraft* editor hosts are rebuilt against a post-S04 engine (their imgui imports go to 0).
    python (Join-Path $RepoRoot "scripts\audit_dll_exports.py") --verify-coverage @ExternalArgs --transitional-missing-bucket imgui
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Push-Location $BinDir
try {
    Write-Host "verify: minEngineTests.exe test smoke ..."
    & ".\minEngineTests.exe" test smoke
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
