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

if(TARGET MaximumEditorLib)
    minengine_add_editor_executable(SampleEditor
        MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../Editor/src/EditorMain.cpp"
        GAME_MODULE GameModuleStub
        OUTPUT_NAME SampleEditor
    )
endif()
