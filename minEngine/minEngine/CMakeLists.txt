# Runtime sources are collected recursively (includes MaterialCompiler: MaterialCompileTypes,
# MaterialCompiler (facades + types); GLSL/ backend impls).
# Generated/*.gen.cpp are attached by minengine_add_reflection_codegen (CORE-F27).
file(GLOB_RECURSE SRC CONFIGURE_DEPENDS ./src/*.cpp)
list(FILTER SRC EXCLUDE REGEX ".*/Runtime/Core/Serialization/Legacy/.*")
list(FILTER SRC EXCLUDE REGEX ".*/Runtime/Core/Reflection/Legacy/.*")
list(FILTER SRC EXCLUDE REGEX ".*/Generated/.*")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(NOT DEFINED MINENGINE_CMAKE_DIR)
    set(MINENGINE_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
endif()
if(NOT DEFINED MINENGINE_HEADER_TOOL)
    set(MINENGINE_HEADER_TOOL "${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/minEngine_header_tool.py")
endif()
include("${MINENGINE_CMAKE_DIR}/MinEngineReflection.cmake")
include("${MINENGINE_CMAKE_DIR}/MinEngineExports.cmake")

set(REFLECTION_SCAN_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(REFLECTION_GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/Generated/Reflection)
set(SCRIPT_BINDING_GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/Generated/ScriptBinding)

# BUILD-F04 S04: the imgui family (core, glfw/opengl3/vulkan backends, misc/freetype loader,
# ImGuizmo) is compiled by the EDITOR DLL now - see Editor/CMakeLists.txt. The engine src never
# called it (0 ImGui:: calls, 0 GImGui, 0 imgui_internal.h); the 253 exports existed only so the
# editor hosts could reach imgui THROUGH the engine DLL (BUILD-F03 C2 forbids them here).
add_library(${ENGINE_NAME} SHARED ${SRC}
                                  ${THIRD_PARTY_PATH}/glad/src/glad.c
                                  ${THIRD_PARTY_PATH}/stb_image/stb_image.cpp
                                  ${CMAKE_CURRENT_SOURCE_DIR}/Third-Party/miniaudio/miniaudio.c
                                  )

minengine_add_reflection_codegen(${ENGINE_NAME}
    SRC_ROOT ${REFLECTION_SCAN_ROOT}
    OUT_DIR ${REFLECTION_GENERATED_DIR}
    MANIFEST ${REFLECTION_GENERATED_DIR}/manifest.json
    SCRIPT_BINDING_OUT_DIR ${SCRIPT_BINDING_GENERATED_DIR}
    SCRIPT_BINDING_MANIFEST ${SCRIPT_BINDING_GENERATED_DIR}/manifest.json
    CODEGEN_TARGET_NAME reflection_codegen
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    EXCLUDE_REGEX
        ".*/Runtime/Core/Serialization/Legacy/.*"
        ".*/Runtime/Core/Reflection/Legacy/.*"
)

set(NFD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Third-Party/nativefiledialog-extended)
if(EXISTS ${NFD_DIR}/CMakeLists.txt)
    add_subdirectory(${NFD_DIR} ${CMAKE_BINARY_DIR}/third-party/nfd EXCLUDE_FROM_ALL)
    target_link_libraries(${ENGINE_NAME} PRIVATE nfd)
endif()

# Assimp (mesh importer): prefer prebuilt DLL from bin, fallback to subproject.
set(ASSIMP_PREBUILT_DLL "${EXECUTABLE_PATH}/libassimp-6.dll")
set(ASSIMP_LINK_TARGET "")
if(EXISTS ${ASSIMP_PREBUILT_DLL})
    # MinGW can link against DLL directly with full path.
    set(ASSIMP_LINK_TARGET "${ASSIMP_PREBUILT_DLL}")
    # Prebuilt path skips assimp's CMake configure; headers still need config.h.
    set(_ASSIMP_CONFIG_H "${THIRD_PARTY_PATH}/assimp/include/assimp/config.h")
    set(_ASSIMP_CONFIG_H_IN "${THIRD_PARTY_PATH}/assimp/include/assimp/config.h.in")
    if(EXISTS "${_ASSIMP_CONFIG_H_IN}" AND NOT EXISTS "${_ASSIMP_CONFIG_H}")
        file(READ "${_ASSIMP_CONFIG_H_IN}" _ASSIMP_CONFIG_H_CONTENT)
        string(REGEX REPLACE "#cmakedefine[^\n]*\n" "" _ASSIMP_CONFIG_H_CONTENT "${_ASSIMP_CONFIG_H_CONTENT}")
        file(WRITE "${_ASSIMP_CONFIG_H}" "${_ASSIMP_CONFIG_H_CONTENT}")
    endif()
else()
    set(ASSIMP_DIR ${THIRD_PARTY_PATH}/assimp)
    if(EXISTS ${ASSIMP_DIR}/CMakeLists.txt)
        set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
        set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
        add_subdirectory(${ASSIMP_DIR} ${CMAKE_BINARY_DIR}/third-party/assimp EXCLUDE_FROM_ALL)
        set(ASSIMP_LINK_TARGET assimp)
    endif()
endif()

set(FREETYPE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Third-Party/freetype)
if(EXISTS ${FREETYPE_DIR}/CMakeLists.txt)
    add_subdirectory(${FREETYPE_DIR} ${CMAKE_BINARY_DIR}/third-party/freetype EXCLUDE_FROM_ALL)
    target_link_libraries(${ENGINE_NAME} PRIVATE freetype)
endif()

# Lua + sol2 (CORE-F01) — vendored under Third-Party (see lua/VENDOR.md, sol2/VENDOR.md).
# Custom static target: do not use upstream Lua CMake (too old for CMake 4).
set(LUA_DIR ${THIRD_PARTY_PATH}/lua)
set(LUA_SRC_DIR ${LUA_DIR}/src)
set(LUA_INCLUDE_DIR ${LUA_DIR}/include)
if(NOT EXISTS ${LUA_INCLUDE_DIR}/lua.h)
    message(FATAL_ERROR "Vendored Lua not found at ${LUA_DIR}. See Third-Party/lua/VENDOR.md.")
endif()
file(GLOB LUA_LIB_SOURCES CONFIGURE_DEPENDS ${LUA_SRC_DIR}/*.c)
list(FILTER LUA_LIB_SOURCES EXCLUDE REGEX ".*/lua\\.c$")
list(FILTER LUA_LIB_SOURCES EXCLUDE REGEX ".*/luac\\.c$")
add_library(minengine_lua STATIC ${LUA_LIB_SOURCES})
target_include_directories(minengine_lua
    PUBLIC ${LUA_INCLUDE_DIR}
    PRIVATE ${LUA_SRC_DIR}
)
set_target_properties(minengine_lua PROPERTIES
    C_STANDARD 99
    POSITION_INDEPENDENT_CODE ON
)

set(SOL2_INCLUDE_DIR ${THIRD_PARTY_PATH}/sol2/include)
if(NOT EXISTS ${SOL2_INCLUDE_DIR}/sol/sol.hpp)
    message(FATAL_ERROR "Vendored sol2 not found at ${THIRD_PARTY_PATH}/sol2. See Third-Party/sol2/VENDOR.md.")
endif()

target_link_libraries(${ENGINE_NAME} PUBLIC minengine_lua)
target_include_directories(${ENGINE_NAME} PUBLIC ${SOL2_INCLUDE_DIR})

# Jolt Physics (rigid body simulation). Headers stay in Physics/; link only here for S01-a.
set(JOLT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Third-Party/Jolt)
if(EXISTS ${JOLT_DIR}/Build/CMakeLists.txt)
    set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
    set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
    set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
    set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
    add_subdirectory(${JOLT_DIR}/Build ${CMAKE_BINARY_DIR}/third-party/jolt EXCLUDE_FROM_ALL)

    # Jolt is an upstream submodule we do not patch, and it exports its instruction-set
    # options as INTERFACE options, so linking it silently compiles every one of our own
    # translation units with AVX. At -O0 GCC then copies large by-value aggregates with a
    # 32-byte aligned vector store, which faults whenever the frame is 16-byte aligned -
    # the only alignment the Win64 ABI guarantees. Scrub the wide-vector options here: the
    # Jolt library still builds itself with AVX2, our own code stays on the SSE baseline.
    get_target_property(_jolt_interface_options Jolt INTERFACE_COMPILE_OPTIONS)
    if(_jolt_interface_options)
        list(FILTER _jolt_interface_options EXCLUDE REGEX "^-m(avx|avx2|avx512[a-z]*|f16c|fma)")
        set_target_properties(Jolt PROPERTIES INTERFACE_COMPILE_OPTIONS "${_jolt_interface_options}")
    endif()
    # Jolt headers reach into SSE4.x intrinsics behind their PUBLIC feature macros, so the
    # SSE baseline has to come back explicitly; SSE4.2 does not imply AVX.
    target_compile_options(Jolt INTERFACE -msse4.2)

    target_link_libraries(${ENGINE_NAME} PRIVATE Jolt)
else()
    message(FATAL_ERROR "Jolt Physics not found. Initialize submodule: git submodule update --init minEngine/minEngine/Third-Party/Jolt")
endif()

set_target_properties(${ENGINE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
    LIBRARY_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
    ARCHIVE_OUTPUT_DIRECTORY ${EXECUTABLE_PATH} 
)

# BUILD-F02 / S01: PE export contract. See cmake/MinEngineExports.cmake; the pre-S01
# behaviour (--export-all-symbols, 65533 exports, 2 ordinals of headroom) is still
# reachable with -DMINENGINE_EXPORT_MODE=auto.
minengine_apply_export_mode(${ENGINE_NAME})

# O1/O2/O3 on demand: report the export table, its composition and per-consumer coverage.
if(WIN32 AND EXISTS "${MINENGINE_AUDIT_SCRIPT}" AND Python3_EXECUTABLE)
    add_custom_target(export-audit
        COMMAND ${Python3_EXECUTABLE} "${MINENGINE_AUDIT_SCRIPT}"
                --dll "$<TARGET_FILE:${ENGINE_NAME}>" --report --verify-coverage
        DEPENDS ${ENGINE_NAME}
        COMMENT "minEngine export audit (report + consumer coverage)"
        VERBATIM)
endif()

# BUILD-F04 S04: IMGUI_ENABLE_FREETYPE moved to the editor target, where it is PUBLIC so that
# imgui core, imgui_freetype.cpp and imgui-node-editor see one configuration (design 4.6 -
# before this slice node-editor compiled without it, i.e. with IMGUI_ENABLE_STB_TRUETYPE).
target_compile_definitions(${ENGINE_NAME} PRIVATE MINENGINE_BUILD)

target_include_directories(${ENGINE_NAME} PUBLIC ./src
                                          PUBLIC ./src/Runtime
                                          PUBLIC ./src/Runtime/Core
                                          PUBLIC ./src/Runtime/Function
                                          PUBLIC ./src/Runtime/Platform/FileDialog
                                          PUBLIC ${REFLECTION_GENERATED_DIR}
                                          PUBLIC ${THIRD_PARTY_PATH}
                                          PUBLIC ${THIRD_PARTY_PATH}/spdlog/include
                                          PUBLIC ${THIRD_PARTY_PATH}/glfw/include
                                          PUBLIC ${THIRD_PARTY_PATH}/glad/include
                                          PUBLIC ${THIRD_PARTY_PATH}/stb_image
                                          PUBLIC ${THIRD_PARTY_PATH}/assimp/include
                                          PUBLIC ${THIRD_PARTY_PATH}/json
                                          PUBLIC ${THIRD_PARTY_PATH}/CLI11
                                          PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Third-Party/miniaudio
                                          )

if(ASSIMP_LINK_TARGET STREQUAL "")
    message(FATAL_ERROR "Assimp library not found. Put libassimp-6.dll in ${EXECUTABLE_PATH} or provide Third-Party/assimp source.")
endif()

# BUILD-F04 S04: glfw is a shared dependency now (minengine_glfw = prebuilt glfw3.dll), not the
# static libglfw3.a that used to make the engine DLL glfw's only instance. The editor DLL imports
# the same glfw3.dll, so there is exactly one GLFW state per process.
target_link_libraries(${ENGINE_NAME} PRIVATE minengine_glfw opengl32 ${ASSIMP_LINK_TARGET})

# Both libminEngined.dll and libMaximumEditord.dll import glfw3.dll, so it must be paired with
# them at runtime. A missing copy is a loud loader failure (0xC0000135), never a silent fallback.
add_custom_command(TARGET ${ENGINE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MINENGINE_GLFW_DIR}/glfw3.dll" "${EXECUTABLE_PATH}/glfw3.dll"
    COMMENT "pair glfw3.dll with the engine DLL"
    VERBATIM)

target_link_directories(${ENGINE_NAME} PUBLIC ${THIRD_PARTY_PATH}/glfw/lib-mingw-w64)

# RND-F05: Vulkan SDK (headers/libs later; glslangValidator for ShaderCompiler).
find_package(Vulkan QUIET)
find_program(MINENGINE_GLSLANG_VALIDATOR
    NAMES glslangValidator glslang
    HINTS
        ENV VULKAN_SDK
        ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}
    PATH_SUFFIXES Bin bin
)
if(MINENGINE_GLSLANG_VALIDATOR)
    message(STATUS "RND-F05: glslangValidator = ${MINENGINE_GLSLANG_VALIDATOR}")
    # Escape backslashes for stringize in C++.
    file(TO_CMAKE_PATH "${MINENGINE_GLSLANG_VALIDATOR}" MINENGINE_GLSLANG_VALIDATOR_CMAKE)
    string(REPLACE "\\" "/" MINENGINE_GLSLANG_VALIDATOR_CMAKE "${MINENGINE_GLSLANG_VALIDATOR_CMAKE}")
    target_compile_definitions(${ENGINE_NAME} PRIVATE
        MINENGINE_GLSLANG_VALIDATOR="${MINENGINE_GLSLANG_VALIDATOR_CMAKE}"
    )
else()
    message(WARNING "RND-F05: glslangValidator not found. Set VULKAN_SDK. ShaderCompiler will fail at runtime until discovered.")
endif()
if(Vulkan_FOUND)
    message(STATUS "RND-F05: Vulkan package found (${Vulkan_VERSION})")
    target_include_directories(${ENGINE_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(${ENGINE_NAME} PRIVATE Vulkan::Vulkan)
    target_compile_definitions(${ENGINE_NAME} PUBLIC MINENGINE_HAS_VULKAN=1)
    # BUILD-F04 S04: imgui_impl_vulkan.cpp moved to the editor DLL (design 4.7). Vulkan::Vulkan
    # and the include dirs stay PUBLIC here so the editor inherits them.
else()
    message(WARNING "RND-F05: find_package(Vulkan) failed. --rhi vulkan will not initialize.")
endif()

# BUILD-F04 S04 / invariant I-gimgui: GImGui is a plain global in imgui.cpp, so imgui's compiling
# unit may exist in exactly ONE target. A second copy is a second ImGuiContext (IM_ASSERT hits, id
# collisions, split font atlas) and - unlike an export - it does NOT show up in the export table,
# which is why this asserts the source list instead of only the export bucket. Runs before the
# link, so "imgui quietly back in the engine" is a configure error, not a runtime crash report.
get_target_property(MINENGINE_ENGINE_SOURCES ${ENGINE_NAME} SOURCES)
foreach(_engine_source IN LISTS MINENGINE_ENGINE_SOURCES)
    if(_engine_source MATCHES "(^|/)(imgui|imgui-upstream|imgui-node-editor|ImGuizmo)(/|$)")
        message(FATAL_ERROR
            "I-gimgui violated (BUILD-F04 S04): the engine target compiles '${_engine_source}'. "
            "The imgui family belongs to MaximumEditorLib (Editor/CMakeLists.txt); a second copy "
            "in the engine DLL means two GImGui contexts in one process. See "
            "docs/ai/Platform/ProductBuild/BUILD-F04_S04_IMGUI_RELOCATION_IMPLEMENTATION.md")
    endif()
endforeach()
