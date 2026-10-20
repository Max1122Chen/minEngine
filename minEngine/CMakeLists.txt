cmake_minimum_required(VERSION 3.20)

project(minEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Nested product projects may pick MSVC; spdlog/fmt require UTF-8 source charset.
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Application targets
option(BUILD_PLAYGROUND "Build the Playground game target" OFF)
option(BUILD_EDITOR "Build the Editor target" ON)
option(BUILD_GAME_MODULE_SAMPLE "Build Examples GameModuleStub + SampleGame/SampleEditor" ON)
option(BUILD_TESTS "Build minEngineTests headless test executable" ON)

# AddressSanitizer (Clang/GCC with ASan runtime). Use a separate build dir (build-asan).
# Current MinGW-Builds GCC 15 lacks libasan — use LLVM-MinGW (UCRT) clang++ instead.
option(MINENGINE_ENABLE_ASAN "Enable AddressSanitizer for Editor/engine (+ source third-parties)" OFF)

set(ENGINE_NAME minEngine)
set(THIRD_PARTY_PATH ${PROJECT_SOURCE_DIR}/${ENGINE_NAME}/Third-Party)

if(MINENGINE_ENABLE_ASAN)
    if(NOT DEFINED MINENGINE_OUTPUT_DIR)
        set(MINENGINE_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/bin-asan")
    endif()
    message(STATUS "MINENGINE_ENABLE_ASAN=ON — output dir: ${MINENGINE_OUTPUT_DIR}")

    # Apply to all targets added after this point (engine, Editor, Jolt, freetype, lua, …).
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)

    # efsw uses basic_string<unsigned int>; LLVM libc++ lacks char_traits<unsigned int>.
    # Only force-include for C++ TUs (C sources would break on the .hpp).
    set(_MINENGINE_ASAN_SHIM "${PROJECT_SOURCE_DIR}/../scripts/debug/asan_char_traits_shim.hpp")
    if(EXISTS "${_MINENGINE_ASAN_SHIM}")
        file(TO_CMAKE_PATH "${_MINENGINE_ASAN_SHIM}" _MINENGINE_ASAN_SHIM_NORM)
        add_compile_options(
            "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include \"${_MINENGINE_ASAN_SHIM_NORM}\">"
        )
        message(STATUS "MINENGINE_ENABLE_ASAN: CXX force-include ${_MINENGINE_ASAN_SHIM_NORM}")
    endif()
endif()

if(DEFINED MINENGINE_OUTPUT_DIR AND NOT "${MINENGINE_OUTPUT_DIR}" STREQUAL "")
    set(EXECUTABLE_PATH "${MINENGINE_OUTPUT_DIR}")
else()
    set(EXECUTABLE_PATH "${PROJECT_SOURCE_DIR}/bin")
endif()
# CACHE so a parent project (GameProject template) can see it after add_subdirectory.
set(EXECUTABLE_PATH "${EXECUTABLE_PATH}" CACHE PATH "Runtime output directory for engine/app binaries" FORCE)

# Extra suites compiled into minEngineTests (empty = off).
set(MINENGINE_EXTRA_TEST_SOURCES "" CACHE STRING
    "Semicolon-separated extra .cpp files compiled into minEngineTests (must define RegisterExtraTestSuites).")
set(MINENGINE_EXTRA_TEST_DIR "" CACHE PATH
    "Optional directory; all *.cpp under it are appended to MINENGINE_EXTRA_TEST_SOURCES.")
set(MINENGINE_EXTRA_TEST_INCLUDE_DIRS "" CACHE STRING
    "Semicolon-separated extra include dirs for minEngineTests when EXTRA tests are enabled.")
set(MINENGINE_EXTRA_TEST_LINK_LIBRARIES "" CACHE STRING
    "Semicolon-separated CMake targets/libs to link into minEngineTests for EXTRA suites.")

# Shared by engine modules and external game SHARED targets (reflection codegen helpers).
set(MINENGINE_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake" CACHE PATH
    "Directory containing MinEngineReflection.cmake and related helpers.")
set(MINENGINE_HEADER_TOOL "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/minEngine_header_tool.py" CACHE FILEPATH
    "Path to minEngine_header_tool.py")

# BUILD-F04 S04: the imgui family belongs to the editor DLL, but its HEADERS still have two
# consumers - the editor DLL (which compiles imgui) and minEngineTests (which compiles a curated
# subset of Editor/src/**). Before this slice the engine target's PUBLIC include dirs were the
# accidental provider, which is exactly why removing them from the engine broke minEngineTests
# with "imgui.h: No such file or directory". Naming the header interface once keeps "who may
# include imgui.h" explicit instead of re-adding three -I lines per target.
# IMGUI_ENABLE_FREETYPE rides along so every TU that includes imgui sees ONE configuration
# (design 4.6; before S04 imgui-node-editor compiled without it).
add_library(minengine_imgui_headers INTERFACE)
target_include_directories(minengine_imgui_headers INTERFACE
    ${THIRD_PARTY_PATH}/imgui
    ${THIRD_PARTY_PATH}/imgui-upstream/misc/freetype
    ${THIRD_PARTY_PATH}/ImGuizmo
)
target_compile_definitions(minengine_imgui_headers INTERFACE IMGUI_ENABLE_FREETYPE)

# BUILD-F04 S04 (= BUILD-F03 slice 1 step 1, pulled forward because S04 cannot link without it):
# glfw becomes a SHARED dependency. imgui_impl_glfw.cpp needs ~48 glfw symbols, and glfw's single
# process-wide instance used to live INSIDE libminEngined.dll (static libglfw3.a: the engine defines
# the 8 glfw exports and imports no glfw3.dll - measured). A second static copy in the editor DLL
# would split GLFW window/callback/input state, which is worse than two GImGui contexts. One
# glfw3.dll, imported by the engine and by the editor DLL, keeps exactly one instance; the side
# effect is that the glfw bucket leaves the engine export contract (BUILD-F03 C1).
set(MINENGINE_GLFW_DIR "${THIRD_PARTY_PATH}/glfw/lib-mingw-w64")
if(NOT EXISTS "${MINENGINE_GLFW_DIR}/glfw3.dll" OR NOT EXISTS "${MINENGINE_GLFW_DIR}/libglfw3dll.a")
    message(FATAL_ERROR
        "minEngine needs the prebuilt GLFW DLL: expected glfw3.dll and libglfw3dll.a in "
        "${MINENGINE_GLFW_DIR}. Both DLLs (engine + editor) import glfw3.dll at runtime.")
endif()
# GLOBAL: a project template (Launcher/Templates/GameProject) adds minEngine as a subdirectory and
# still has to list glfw3.dll in minengine_add_package_game(RUNTIME_TARGETS ...).
add_library(minengine_glfw SHARED IMPORTED GLOBAL)
set_target_properties(minengine_glfw PROPERTIES
    IMPORTED_LOCATION "${MINENGINE_GLFW_DIR}/glfw3.dll"
    IMPORTED_IMPLIB "${MINENGINE_GLFW_DIR}/libglfw3dll.a"
    INTERFACE_INCLUDE_DIRECTORIES "${THIRD_PARTY_PATH}/glfw/include"
)

add_subdirectory(${ENGINE_NAME})

if(BUILD_GAME_MODULE_SAMPLE)
    add_subdirectory(Examples/GameModuleStub)
endif()

if(BUILD_EDITOR)
    add_subdirectory(Editor)
endif()

if(BUILD_GAME_MODULE_SAMPLE)
    add_subdirectory(Examples/SampleGameHost)
endif()

if(BUILD_TESTS)
    add_subdirectory(Tests)
endif()
