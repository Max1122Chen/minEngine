#!/usr/bin/env bash
# Builds and runs the shared-per-frame-block probe.
#
#   build_and_run.sh             # current tree
#   build_and_run.sh <dir>       # <dir> holds ScreenUIPass.{h,cpp} and Access/ScreenUIPassTestAccess.h
#
# The engine library exports neither ScreenUIPass nor the per-object binder, so the probe compiles
# ScreenUIPass.cpp itself and links the rest of the engine from the contract build.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
BUILD="$ROOT/build"
OUT="$BUILD/_screenui_probe"
CXX="${CXX:-g++}"

SRC_DIR="${1:-}"
INCS="-I$HERE -I$ROOT/Tests -I$ROOT/minEngine/src -I$ROOT/minEngine/src/Runtime"
# Engine headers next to the pipeline include each other by bare name ("RenderPasses/ShadowPass.h");
# the compiler finds those through the directory of the including file, which for a snapshot build is
# the snapshot. Point it back at the real pipeline directory for everything the snapshot does not carry.
INCS="$INCS -I$ROOT/minEngine/src/Runtime/Function/Render/RenderPipeline"
if [ -n "$SRC_DIR" ]; then
    INCS="-I$SRC_DIR $INCS"
fi
if [ -f "$BUILD/Tests/CMakeFiles/minEngineTests.dir/includes_CXX.rsp" ]; then
    INCS="$INCS $(tr ' ' '\n' < "$BUILD/Tests/CMakeFiles/minEngineTests.dir/includes_CXX.rsp" | sed 's/"//g' | tr '\n' ' ')"
fi

PASS_SRC="$ROOT/minEngine/src/Runtime/Function/Render/RenderPipeline/RenderPasses/ScreenUIPass.cpp"
if [ -n "$SRC_DIR" ]; then
    PASS_SRC="$SRC_DIR/Runtime/Function/Render/RenderPipeline/RenderPasses/ScreenUIPass.cpp"
fi

mkdir -p "$OUT"

# glad is compiled into the engine but not exported, so the probe carries its own copy.
if [ ! -f "$OUT/glad.o" ]; then
    ${CC:-gcc} -O0 -g -c "$ROOT/minEngine/Third-Party/glad/src/glad.c" \
        -I"$ROOT/minEngine/Third-Party/glad/include" -o "$OUT/glad.o" || exit 2
fi

BINDING_SETS_SRC="$ROOT/minEngine/src/Runtime/Function/Render/EngineSceneBindingSets.cpp"
if [ -n "$SRC_DIR" ] && [ -f "$SRC_DIR/Runtime/Function/Render/EngineSceneBindingSets.cpp" ]; then
    BINDING_SETS_SRC="$SRC_DIR/Runtime/Function/Render/EngineSceneBindingSets.cpp"
fi

echo "== compiling ($PASS_SRC)"
$CXX -g -std=c++17 -DMINENGINE_HAS_VULKAN=1 $INCS \
    "$OUT/glad.o" \
    "$HERE/main.cpp" \
    "$PASS_SRC" \
    "$BINDING_SETS_SRC" \
    "$ROOT/minEngine/src/Runtime/Function/Render/RHI/RHITextureViewCache.cpp" \
    "$HERE/SceneMeshDrawStubs.cpp" \
    -o "$OUT/ScreenUIPerFrameUboProbe.exe" \
    "$ROOT/bin/libminEngined.dll.a" \
    "$ROOT/minEngine/Third-Party/glfw/lib-mingw-w64/libglfw3dll.a" \
    -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32 \
    || exit 2

echo "== running"
PATH="$ROOT/bin:$PATH" "$OUT/ScreenUIPerFrameUboProbe.exe"
