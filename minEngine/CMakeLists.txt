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

add_subdirectory(${ENGINE_NAME})
 

if(BUILD_EDITOR) 
	add_subdirectory(Editor)
endif()

option(BUILD_TESTS "Build minEngineTests headless test executable" ON)
if(BUILD_TESTS)
    add_subdirectory(Tests)
endif()


