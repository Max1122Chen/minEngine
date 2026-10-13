cmake_minimum_required(VERSION 3.20)

project({{PROJECT_NAME}} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ENGINE_ROOT "{{ENGINE_ROOT}}" CACHE PATH
    "minEngine repository root (contains the minEngine/ CMake project)")
if(NOT EXISTS "${ENGINE_ROOT}/minEngine/CMakeLists.txt")
    message(FATAL_ERROR
        "ENGINE_ROOT must point at the minEngine repository root. "
        "Missing: ${ENGINE_ROOT}/minEngine/CMakeLists.txt")
endif()

# Project-local bin/; disable in-tree sample/tests when pulling the engine.
set(MINENGINE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin" CACHE PATH "" FORCE)
set(BUILD_GAME_MODULE_SAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PLAYGROUND OFF CACHE BOOL "" FORCE)

add_subdirectory("${ENGINE_ROOT}/minEngine" "${CMAKE_BINARY_DIR}/engine")

# Parent scope may not inherit non-CACHE vars from the engine tree; prefer CACHE / output dir.
if(NOT DEFINED EXECUTABLE_PATH OR "${EXECUTABLE_PATH}" STREQUAL "")
    set(EXECUTABLE_PATH "${MINENGINE_OUTPUT_DIR}")
endif()

if(NOT EXISTS "${MINENGINE_CMAKE_DIR}/MinEngineTargets.cmake")
    message(FATAL_ERROR "MinEngineTargets.cmake not found under engine cmake/")
endif()
include("${MINENGINE_CMAKE_DIR}/MinEngineTargets.cmake")
include("${MINENGINE_CMAKE_DIR}/MinEngineReflection.cmake")

set(GAME_MODULE_NAME {{GAME_MODULE_NAME}})
set(GAME_MODULE_SRC_ROOT "${CMAKE_SOURCE_DIR}/Source/${GAME_MODULE_NAME}")
set(GAME_MODULE_GENERATED_DIR "${CMAKE_SOURCE_DIR}/Generated/Reflection")

add_library(${GAME_MODULE_NAME} SHARED
    "${GAME_MODULE_SRC_ROOT}/Module.cpp"
)
set_target_properties(${GAME_MODULE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_PATH}"
    LIBRARY_OUTPUT_DIRECTORY "${EXECUTABLE_PATH}"
    ARCHIVE_OUTPUT_DIRECTORY "${EXECUTABLE_PATH}"
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(MINGW)
    target_link_options(${GAME_MODULE_NAME} PRIVATE "LINKER:--export-all-symbols")
endif()
target_compile_definitions(${GAME_MODULE_NAME} PRIVATE {{GAME_MODULE_NAME}}_BUILD=1)
target_include_directories(${GAME_MODULE_NAME} PUBLIC "${GAME_MODULE_SRC_ROOT}")
target_link_libraries(${GAME_MODULE_NAME} PRIVATE minEngine)

minengine_add_reflection_codegen(${GAME_MODULE_NAME}
    SRC_ROOT "${GAME_MODULE_SRC_ROOT}"
    OUT_DIR "${GAME_MODULE_GENERATED_DIR}"
    MANIFEST "${GAME_MODULE_GENERATED_DIR}/manifest.json"
    CODEGEN_TARGET_NAME ${GAME_MODULE_NAME}_reflection_codegen
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

minengine_add_game_executable({{PROJECT_NAME}}
    SOURCES "${CMAKE_SOURCE_DIR}/Source/Programs/GameMain.cpp"
    GAME_MODULE ${GAME_MODULE_NAME}
    OUTPUT_NAME {{PROJECT_NAME}}
)

minengine_add_editor_executable({{PROJECT_EDITOR_NAME}}
    MAIN_SOURCE "${ENGINE_ROOT}/minEngine/Editor/src/EditorMain.cpp"
    GAME_MODULE ${GAME_MODULE_NAME}
    OUTPUT_NAME {{PROJECT_EDITOR_NAME}}
)

minengine_add_package_game({{PROJECT_NAME}}
    STAGE_DIR "${CMAKE_SOURCE_DIR}/Staging/{{PROJECT_NAME}}"
    CONTENT_DIR "${CMAKE_SOURCE_DIR}/Assets"
    RUNTIME_TARGETS ${GAME_MODULE_NAME} minEngine
)
