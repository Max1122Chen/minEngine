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
# BUILD-F03 knife 1: and NO glad entry either. The engine still compiles glad.c, but it must not
# EXPORT it: minEngineTests carries its own glad translation unit now, and a second exported glad
# copy is the same two-instances shape as glfw.
python (Join-Path $RepoRoot "scripts\audit_dll_exports.py") --max-exports 3000 --fail --forbid-buckets imgui,glfw,glad
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# BUILD-F03 knife 1 removal guard: the assertions above bound what may APPEAR in the export table;
# none of them notices a symbol DISAPPEARING, which is exactly what breaks an external product
# repo on its next link. Every symbol in the frozen baseline must still be exported. Deleting one
# on purpose means refreshing the baseline, so the removal appears in the commit diff
# (scripts\audit_dll_exports.py --write-export-baseline ...) - there is no allow-removal flag.
# A missing/empty baseline fails here; it never degrades to "nothing to compare".
python (Join-Path $RepoRoot "scripts\audit_dll_exports.py") --dll (Join-Path $BinDir "libminEngined.dll") --assert-no-removals-vs (Join-Path $RepoRoot "minEngine\cmake\minEngine_export_baseline.json")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# BUILD-F04 S04 / O3 in-repo half: every in-repo consumer of the post-S04 engine contract must
# still resolve. This one is never tolerated - it is the assertion that the 253 removed imgui
# entries were not a symbol some in-repo artifact actually needs from the engine.
python (Join-Path $RepoRoot "scripts\audit_dll_exports.py") --dll (Join-Path $BinDir "libminEngined.dll") --verify-coverage
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# BUILD-F03 knife 1: with glad out of the export table the engine initializes its OWN GL function
# table (OpenGLFunctionTable.h), because glad's pointers are per module. That rule is mechanical
# here: every gl*-calling function in the OpenGL backend must call
# EnsureOpenGLFunctionTableLoaded() first, and no other engine source may call gl* at all.
# Without this gate the rule would depend on the next author remembering it, and the failure mode
# is a null function pointer reached only by some paths.
python (Join-Path $RepoRoot "scripts\audit_opengl_load_guard.py")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# BUILD-F04 S01/S02/S03: editor/module boundary. Every assertion starts with "does the artifact
# exist", so a missing libMaximumEditord.dll / editor exe fails loudly instead of skipping checks.
python (Join-Path $RepoRoot "scripts\audit_module_boundaries.py") --assert --fail
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# BUILD-F04 S02: both editor hosts must resolve every import from the editor DLL's export contract.
python (Join-Path $RepoRoot "scripts\audit_dll_exports.py") --dll (Join-Path $BinDir "libMaximumEditord.dll") --verify-coverage
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# BUILD-F04 S05: every editor host must carry its compile-time game module identity into the
# shared editor DLL (minengine_add_editor_executable -> MINENGINE_EDITOR_GAME_MODULE /
# MINENGINE_EDITOR_ASSET_ONLY -> main.h -> SetHostEditorGameModule). Asserted on the PE import
# tables, so it needs no desktop: if the injection is dropped, the import disappears and this
# fails here instead of at the first user who opens the wrong project with the wrong editor.
# The behavioural half (the four-case gate matrix) launches the editor and is its own script:
#   python scripts/smoke_editor_module_gate.py
python (Join-Path $RepoRoot "scripts\smoke_editor_module_gate.py") --case identity
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
