# Runtime sources are collected recursively (includes MaterialCompiler: MaterialCompileTypes,
# MaterialCompiler (facades + types); GLSL/ backend impls).
file(GLOB_RECURSE SRC CONFIGURE_DEPENDS ./src/*.cpp)
list(FILTER SRC EXCLUDE REGEX ".*/Runtime/Core/Serialization/Legacy/.*")
list(FILTER SRC EXCLUDE REGEX ".*/Runtime/Core/Reflection/Legacy/.*")

find_package(Python3 REQUIRED COMPONENTS Interpreter)


set(REFLECTION_GENERATOR_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/minEngine_header_tool.py)

set(REFLECTION_SCAN_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(REFLECTION_GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/Generated/Reflection)
set(REFLECTION_GENERATED_MANIFEST ${REFLECTION_GENERATED_DIR}/manifest.json)
file(GLOB_RECURSE REFLECTION_SCAN_HEADERS CONFIGURE_DEPENDS
    ${REFLECTION_SCAN_ROOT}/*.h
    ${REFLECTION_SCAN_ROOT}/*.hpp
)
list(FILTER REFLECTION_SCAN_HEADERS EXCLUDE REGEX ".*/Runtime/Core/Serialization/Legacy/.*")
list(FILTER REFLECTION_SCAN_HEADERS EXCLUDE REGEX ".*/Runtime/Core/Reflection/Legacy/.*")
add_custom_command(
    OUTPUT ${REFLECTION_GENERATED_MANIFEST}
    COMMAND ${Python3_EXECUTABLE} ${REFLECTION_GENERATOR_SCRIPT}
            --src-root ${REFLECTION_SCAN_ROOT}
            --out-dir ${REFLECTION_GENERATED_DIR}
            --manifest ${REFLECTION_GENERATED_MANIFEST}
    DEPENDS ${REFLECTION_GENERATOR_SCRIPT} ${REFLECTION_SCAN_HEADERS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating reflection registration sources"
    VERBATIM
)

add_custom_target(reflection_codegen DEPENDS ${REFLECTION_GENERATED_MANIFEST})

file(GLOB IMGUI ${THIRD_PARTY_PATH}/imgui/*.cpp
                ${THIRD_PARTY_PATH}/imgui/backends/*.cpp
)
set(IMGUI_UPSTREAM_DIR ${THIRD_PARTY_PATH}/imgui-upstream)
set(IMGUI_FREETYPE_SRC ${IMGUI_UPSTREAM_DIR}/misc/freetype/imgui_freetype.cpp)

file(GLOB IMGUIZMO ${THIRD_PARTY_PATH}/ImGuizmo/*.cpp)

add_library(${ENGINE_NAME} SHARED ${SRC}
                                  ${THIRD_PARTY_PATH}/glad/src/glad.c
                                  ${THIRD_PARTY_PATH}/stb_image/stb_image.cpp
                                  ${IMGUI}
                                  ${IMGUI_FREETYPE_SRC}
                                  ${IMGUIZMO}
                                  )

add_dependencies(${ENGINE_NAME} reflection_codegen)

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

set_target_properties(${ENGINE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
    LIBRARY_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
    ARCHIVE_OUTPUT_DIRECTORY ${EXECUTABLE_PATH} 
)

target_compile_definitions(${ENGINE_NAME} PRIVATE MINENGINE_BUILD IMGUI_ENABLE_FREETYPE)

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
                                          PUBLIC ${THIRD_PARTY_PATH}/imgui
                                          PUBLIC ${IMGUI_UPSTREAM_DIR}/misc/freetype
                                          PUBLIC ${THIRD_PARTY_PATH}/json
                                          PUBLIC ${THIRD_PARTY_PATH}/ImGuizmo
                                          PUBLIC ${THIRD_PARTY_PATH}/CLI11
                                          )

if(ASSIMP_LINK_TARGET STREQUAL "")
    message(FATAL_ERROR "Assimp library not found. Put libassimp-6.dll in ${EXECUTABLE_PATH} or provide Third-Party/assimp source.")
endif()

target_link_libraries(${ENGINE_NAME} PRIVATE glfw3 opengl32 ${ASSIMP_LINK_TARGET})

target_link_directories(${ENGINE_NAME} PUBLIC ${THIRD_PARTY_PATH}/glfw/lib-mingw-w64)