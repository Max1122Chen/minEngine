set(MINENGINE_TESTS_NAME minEngineTests)

file(GLOB TEST_SUITE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Suites/*.cpp
)

# ED-F12: DebugCommand + PropertyPath live under Editor; tests compile them without the Editor exe.
file(GLOB_RECURSE EDITOR_DEBUG_COMMAND_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/src/DebugCommand/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/src/PropertyPath/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/src/UI/Property/PropertyEditPolicy.cpp
)

# SPR-F04 / ED-F24: Sprite authoring services + AES + command stack (Editor sources linked into tests).
set(SPRITE_AUTHORING_EDITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/src/Shell/EditorCommandStack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/src/Shell/EditorSettingsDefaults.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/src/Commands/Sprite/EditorSpriteCommands.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/src/Services/Sprite/SpriteEditorServices.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/src/Services/Asset/AssetEditorServices.cpp
)

# BUG-EDITOR-006: overlapped named-pipe interrupt (Editor source linked into tests).
set(AUTOMATION_PIPE_EDITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/src/Automation/AutomationPipeServer.cpp
)

# TEST-F05: optional external / extra suite sources (off by default).
set(_MINENGINE_EXTRA_TEST_SOURCES "${MINENGINE_EXTRA_TEST_SOURCES}")
if(NOT "${MINENGINE_EXTRA_TEST_DIR}" STREQUAL "")
    if(NOT EXISTS "${MINENGINE_EXTRA_TEST_DIR}")
        message(FATAL_ERROR "MINENGINE_EXTRA_TEST_DIR does not exist: ${MINENGINE_EXTRA_TEST_DIR}")
    endif()
    file(GLOB _MINENGINE_EXTRA_TEST_DIR_SOURCES CONFIGURE_DEPENDS
        "${MINENGINE_EXTRA_TEST_DIR}/*.cpp"
    )
    list(APPEND _MINENGINE_EXTRA_TEST_SOURCES ${_MINENGINE_EXTRA_TEST_DIR_SOURCES})
endif()

set(MINENGINE_ENABLE_EXTRA_TESTS OFF)
if(NOT "${_MINENGINE_EXTRA_TEST_SOURCES}" STREQUAL "")
    set(MINENGINE_ENABLE_EXTRA_TESTS ON)
    message(STATUS "TEST-F05: minEngineTests EXTRA sources enabled")
endif()

add_executable(${MINENGINE_TESTS_NAME}
    TestMain.cpp
    TestSuiteRegistration.cpp
    EngineTestFixture.cpp
    DoctestSuiteRunner.cpp
    ${TEST_SUITE_SOURCES}
    ${EDITOR_DEBUG_COMMAND_SOURCES}
    ${SPRITE_AUTHORING_EDITOR_SOURCES}
    ${AUTOMATION_PIPE_EDITOR_SOURCES}
    ${_MINENGINE_EXTRA_TEST_SOURCES}
)

set_target_properties(${MINENGINE_TESTS_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
)

target_include_directories(${MINENGINE_TESTS_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Suites
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Core/Object
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Core/Serialization
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Core/Reflection
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Core/Log
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Function
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Function/Render
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Function/Render/Material
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Function/Render/Material/MaterialIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Function/Render/Material/MaterialCompiler
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Function/Render/Material/MaterialGraphNodeDefs
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Function/Render/Environment
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Function/Render/OpenGL
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Resource
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/Third-Party/doctest
)

if(NOT "${MINENGINE_EXTRA_TEST_INCLUDE_DIRS}" STREQUAL "")
    target_include_directories(${MINENGINE_TESTS_NAME} PRIVATE ${MINENGINE_EXTRA_TEST_INCLUDE_DIRS})
endif()

target_compile_definitions(${MINENGINE_TESTS_NAME} PRIVATE MINENGINE_BUILD)
if(MINENGINE_ENABLE_EXTRA_TESTS)
    target_compile_definitions(${MINENGINE_TESTS_NAME} PRIVATE MINENGINE_ENABLE_EXTRA_TESTS=1)
endif()

target_link_libraries(${MINENGINE_TESTS_NAME} PRIVATE ${ENGINE_NAME})
if(NOT "${MINENGINE_EXTRA_TEST_LINK_LIBRARIES}" STREQUAL "")
    target_link_libraries(${MINENGINE_TESTS_NAME} PRIVATE ${MINENGINE_EXTRA_TEST_LINK_LIBRARIES})
endif()

# When the in-tree sample module is built, prove external ME_CLASS AddComponent in tests.
if(TARGET GameModuleStub)
    target_link_libraries(${MINENGINE_TESTS_NAME} PRIVATE GameModuleStub)
    target_include_directories(${MINENGINE_TESTS_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../Examples/GameModuleStub/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../Examples/GameModuleStub/Generated/Reflection
    )
    target_compile_definitions(${MINENGINE_TESTS_NAME} PRIVATE MINENGINE_LINK_GAME_MODULE_STUB=1)
    message(STATUS "minEngineTests links GameModuleStub for reflection smoke")
endif()

add_dependencies(${MINENGINE_TESTS_NAME} ${ENGINE_NAME})
