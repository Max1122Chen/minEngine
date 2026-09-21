cmake_minimum_required(VERSION 3.20)

# Minimal SHARED used only to prove ED-F20 Editor link hooks.
# Configure the engine with:
#   -DMINENGINE_GAME_DIR=<path-to-this-directory>
#   -DMINENGINE_GAME_TARGET=GameModuleStub

add_library(GameModuleStub SHARED GameModuleStub.cpp)

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
