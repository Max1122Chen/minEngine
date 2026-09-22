cmake_minimum_required(VERSION 3.20)

# Dev-host game SHARED for ED-F20 + CORE-F27 multi-module reflection smoke.
# Configure the engine with:
#   -DMINENGINE_GAME_DIR=<path-to-this-directory>
#   -DMINENGINE_GAME_TARGET=GameModuleStub
#
# Always links minEngine (ED-F21). Runs header tool on this module's src only (CORE-F27).

if(NOT DEFINED MINENGINE_CMAKE_DIR OR NOT EXISTS "${MINENGINE_CMAKE_DIR}/MinEngineReflection.cmake")
    message(FATAL_ERROR
        "GameModuleStub requires MinEngineReflection.cmake. "
        "Configure via the engine tree so MINENGINE_CMAKE_DIR is set "
        "(see docs/ai/Platform/Core/CORE-F27_EXTERNAL_COMPONENT_ATTACH_DESIGN.md).")
endif()
include("${MINENGINE_CMAKE_DIR}/MinEngineReflection.cmake")

set(GAMEMODULESTUB_SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(GAMEMODULESTUB_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Generated/Reflection")

add_library(GameModuleStub SHARED
    GameModuleStub.cpp
    src/StubGamePrimitiveComponent.cpp
)

set_target_properties(GameModuleStub PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_PATH}"
    LIBRARY_OUTPUT_DIRECTORY "${EXECUTABLE_PATH}"
    ARCHIVE_OUTPUT_DIRECTORY "${EXECUTABLE_PATH}"
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

if(MINGW)
    target_link_options(GameModuleStub PRIVATE "LINKER:--export-all-symbols")
endif()

target_compile_definitions(GameModuleStub PRIVATE GAMEMODULESTUB_BUILD=1)
target_include_directories(GameModuleStub PUBLIC "${GAMEMODULESTUB_SRC_ROOT}")

# ED-F21: game module always links the same minEngine SHARED from this configure.
if(NOT TARGET minEngine)
    message(FATAL_ERROR "GameModuleStub expects target 'minEngine' (add_subdirectory after engine).")
endif()
target_link_libraries(GameModuleStub PRIVATE minEngine)

minengine_add_reflection_codegen(GameModuleStub
    SRC_ROOT "${GAMEMODULESTUB_SRC_ROOT}"
    OUT_DIR "${GAMEMODULESTUB_GENERATED_DIR}"
    MANIFEST "${GAMEMODULESTUB_GENERATED_DIR}/manifest.json"
    CODEGEN_TARGET_NAME GameModuleStub_reflection_codegen
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
