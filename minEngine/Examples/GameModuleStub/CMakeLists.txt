cmake_minimum_required(VERSION 3.20)

# Sample game SHARED for in-tree SampleGame/SampleEditor and reflection smoke tests.
# Enabled with BUILD_GAME_MODULE_SAMPLE (default ON).

if(NOT DEFINED MINENGINE_CMAKE_DIR OR NOT EXISTS "${MINENGINE_CMAKE_DIR}/MinEngineReflection.cmake")
    message(FATAL_ERROR
        "GameModuleStub requires MinEngineReflection.cmake. "
        "Configure via the engine tree so MINENGINE_CMAKE_DIR is set.")
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

if(NOT TARGET minEngine)
    message(FATAL_ERROR "GameModuleStub expects target 'minEngine' (add after the engine subdirectory).")
endif()
target_link_libraries(GameModuleStub PRIVATE minEngine)

minengine_add_reflection_codegen(GameModuleStub
    SRC_ROOT "${GAMEMODULESTUB_SRC_ROOT}"
    OUT_DIR "${GAMEMODULESTUB_GENERATED_DIR}"
    MANIFEST "${GAMEMODULESTUB_GENERATED_DIR}/manifest.json"
    CODEGEN_TARGET_NAME GameModuleStub_reflection_codegen
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
