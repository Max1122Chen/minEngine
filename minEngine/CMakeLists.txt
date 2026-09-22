cmake_minimum_required(VERSION 3.20)

project(minEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options for application targets.
option(BUILD_PLAYGROUND "Build the Playground game target" OFF)
option(BUILD_EDITOR "Build the Editor target" ON)

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

if(DEFINED MINENGINE_OUTPUT_DIR)
    set(EXECUTABLE_PATH "${MINENGINE_OUTPUT_DIR}")
else()
    set(EXECUTABLE_PATH ${PROJECT_SOURCE_DIR}/bin)
endif()

# ED-F20: optional link-time game SHARED into MaximumEditor (Dev host). Empty = off.
set(MINENGINE_GAME_DIR "" CACHE PATH
    "External game CMake root (directory with CMakeLists.txt). Empty disables game module link.")
set(MINENGINE_GAME_TARGET "" CACHE STRING
    "CMake SHARED target name to link into Editor when MINENGINE_GAME_DIR is set.")
option(MINENGINE_GAME_COPY_RUNTIME
    "Copy game SHARED runtime next to MaximumEditor when a game module is linked" ON)

# TEST-F05: optional extra suite sources compiled into minEngineTests. Empty = off.
set(MINENGINE_EXTRA_TEST_SOURCES "" CACHE STRING
    "Semicolon-separated extra .cpp files compiled into minEngineTests (must define RegisterExtraTestSuites).")
set(MINENGINE_EXTRA_TEST_DIR "" CACHE PATH
    "Optional directory; all *.cpp under it are appended to MINENGINE_EXTRA_TEST_SOURCES.")
set(MINENGINE_EXTRA_TEST_INCLUDE_DIRS "" CACHE STRING
    "Semicolon-separated extra include dirs for minEngineTests when EXTRA tests are enabled.")
set(MINENGINE_EXTRA_TEST_LINK_LIBRARIES "" CACHE STRING
    "Semicolon-separated CMake targets/libs to link into minEngineTests for EXTRA suites.")

# ED-F20 / CORE-F27: paths for optional game modules that call minengine_add_reflection_codegen.
set(MINENGINE_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake" CACHE PATH
    "Directory containing MinEngineReflection.cmake and related helpers.")
set(MINENGINE_HEADER_TOOL "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/minEngine_header_tool.py" CACHE FILEPATH
    "Path to minEngine_header_tool.py")

add_subdirectory(${ENGINE_NAME})

# Resolve optional game module after the engine target exists (game may link minEngine).
set(MINENGINE_HAS_GAME_MODULE OFF)
if(NOT "${MINENGINE_GAME_DIR}" STREQUAL "")
    if("${MINENGINE_GAME_TARGET}" STREQUAL "")
        message(FATAL_ERROR
            "MINENGINE_GAME_DIR is set (${MINENGINE_GAME_DIR}) but MINENGINE_GAME_TARGET is empty. "
            "Pass -DMINENGINE_GAME_TARGET=<SHARED target name> (see docs/ai/Editor/ED-F20_OPTIONAL_GAME_MODULE_LINK_DESIGN.md).")
    endif()
    if(NOT EXISTS "${MINENGINE_GAME_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "MINENGINE_GAME_DIR does not contain CMakeLists.txt: ${MINENGINE_GAME_DIR}")
    endif()

    add_subdirectory("${MINENGINE_GAME_DIR}" "${CMAKE_BINARY_DIR}/external_game" EXCLUDE_FROM_ALL)

    if(NOT TARGET "${MINENGINE_GAME_TARGET}")
        message(FATAL_ERROR
            "MINENGINE_GAME_TARGET '${MINENGINE_GAME_TARGET}' was not defined by "
            "add_subdirectory(${MINENGINE_GAME_DIR}).")
    endif()

    set(MINENGINE_HAS_GAME_MODULE ON)
    message(STATUS "ED-F20: Editor will link game module target '${MINENGINE_GAME_TARGET}' from ${MINENGINE_GAME_DIR}")
endif()

if(BUILD_EDITOR) 
	add_subdirectory(Editor)
endif()

option(BUILD_TESTS "Build minEngineTests headless test executable" ON)
if(BUILD_TESTS)
    add_subdirectory(Tests)
endif()


