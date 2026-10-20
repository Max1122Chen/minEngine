# In-tree SampleGame (+ SampleEditor when Editor is enabled).

cmake_minimum_required(VERSION 3.20)

if(NOT TARGET GameModuleStub)
    message(FATAL_ERROR "SampleGameHost requires GameModuleStub (add Examples/GameModuleStub first).")
endif()
if(NOT EXISTS "${MINENGINE_CMAKE_DIR}/MinEngineTargets.cmake")
    message(FATAL_ERROR "SampleGameHost requires MinEngineTargets.cmake")
endif()
include("${MINENGINE_CMAKE_DIR}/MinEngineTargets.cmake")

minengine_add_game_executable(SampleGame
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/SampleGameApplication.cpp"
    GAME_MODULE GameModuleStub
    OUTPUT_NAME SampleGame
)

minengine_add_package_game(SampleGame
    STAGE_DIR "${CMAKE_SOURCE_DIR}/Staging/SampleGame"
    # minengine_glfw = the prebuilt glfw3.dll both libminEngined.dll and libMaximumEditord.dll
    # import (BUILD-F04 S04). Without it a staged build starts with 0xC0000135.
    RUNTIME_TARGETS GameModuleStub minEngine minengine_glfw
)

if(TARGET MaximumEditorLib)
    minengine_add_editor_executable(SampleEditor
        MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../Editor/src/EditorMain.cpp"
        GAME_MODULE GameModuleStub
        OUTPUT_NAME SampleEditor
    )
endif()
