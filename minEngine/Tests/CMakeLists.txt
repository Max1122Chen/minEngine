set(MINENGINE_TESTS_NAME minEngineTests)
set(MINENGINE_CRASH_PROBE_NAME minEngineCrashProbe)
set(MINENGINE_CONTRACT_PROBE_NAME minEngineContractProbe)
set(MINENGINE_CONTRACT_PROBE_NAME minEngineContractProbe)

file(GLOB TEST_SUITE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Suites/*.cpp
)

# BUILD-F03 knife 1 (glad): the engine no longer exports the glad loader surface, so the tests
# carry their own glad translation unit. Every suite that touches GL already calls
# gladLoadGLLoader(glfwGetProcAddress) itself, i.e. it never relied on the engine's glad state;
# the DLL keeps its own copy because GLFWWindowSystem.cpp loads GL through it.
set(MINENGINE_TEST_GLAD_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/Third-Party/glad/src/glad.c
)

# Two engine-internal translation units are the subject of a suite, and neither carries
# MINENGINE_API: the environment bake (its recording contract is driven against a device double)
# and the debug draw queues (the world side fills them, the debug draw pass drains them, and that
# drain is what the snapshot suite drives directly). Compiling them into the test binary is what
# makes that possible; nothing in them is exported by the DLL, so the two copies cannot collide at
# link time.
set(MINENGINE_TEST_ENGINE_INTERNAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Function/Render/Environment/EnvMapCapture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Function/Debug/DebugDrawService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src/Runtime/Function/Debug/DebugGeometry.cpp
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

# BUG-EDITOR-008 / BUG-EDITOR-009: the scene property write codec that scene.set_property drives
# (Editor source linked into tests, same shape as the automation pipe above).
set(SCENE_PROPERTY_CODEC_EDITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/src/Services/Scene/ScenePropertyValueCodec.cpp
)

# The crash record cannot be observed from inside the process it is written for: the filter runs
# because the process is going down. The probe is therefore a separate executable that faults on
# request, and the suite reads the record it leaves behind. It compiles the editor's diagnostics
# translation unit directly (the same source the editor DLL carries) so the recorded path is the
# production one; nothing in it is exported, so the two copies cannot collide at link time.
set(MINENGINE_CRASH_DIAGNOSTICS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/src/Platform/EditorCrashDiagnostics.cpp
)

# A fatal assert ends the process it fires in, and a report path that re-entered the log mutex would
# hang it, so neither can be observed from the suite's own process. The probe runs one contract check
# on request under both grades and appends what it did to a trace the suite reads.
set(MINENGINE_CONTRACT_PROBE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/ContractProbe)

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
    ${MINENGINE_TEST_GLAD_SOURCE}
    ${MINENGINE_TEST_ENGINE_INTERNAL_SOURCES}
    ${TEST_SUITE_SOURCES}
    ${EDITOR_DEBUG_COMMAND_SOURCES}
    ${SPRITE_AUTHORING_EDITOR_SOURCES}
    ${AUTOMATION_PIPE_EDITOR_SOURCES}
    ${SCENE_PROPERTY_CODEC_EDITOR_SOURCES}
    ${MINENGINE_CRASH_DIAGNOSTICS_SOURCES}
    ${_MINENGINE_EXTRA_TEST_SOURCES}
)

# Built before the tests so the suite always finds its probe next to minEngineTests.exe.
add_executable(${MINENGINE_CRASH_PROBE_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/CrashRecordProbe/main.cpp
    ${MINENGINE_CRASH_DIAGNOSTICS_SOURCES}
)
set_target_properties(${MINENGINE_CRASH_PROBE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
)
target_include_directories(${MINENGINE_CRASH_PROBE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Editor/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src
)
target_link_libraries(${MINENGINE_CRASH_PROBE_NAME} PRIVATE ${ENGINE_NAME})
add_dependencies(${MINENGINE_CRASH_PROBE_NAME} ${ENGINE_NAME})

# Built before the tests so the suite always finds its probe next to minEngineTests.exe.
add_executable(${MINENGINE_CONTRACT_PROBE_NAME}
    ${MINENGINE_CONTRACT_PROBE_DIR}/main.cpp
    ${MINENGINE_CONTRACT_PROBE_DIR}/ReleaseGradeProbe.cpp
)
set_target_properties(${MINENGINE_CONTRACT_PROBE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
)
target_include_directories(${MINENGINE_CONTRACT_PROBE_NAME} PRIVATE
    ${MINENGINE_CONTRACT_PROBE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/src
)
target_link_libraries(${MINENGINE_CONTRACT_PROBE_NAME} PRIVATE ${ENGINE_NAME})
add_dependencies(${MINENGINE_CONTRACT_PROBE_NAME} ${ENGINE_NAME})

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

# MINENGINE_BUILD must NOT be defined here: it selects __declspec(dllexport) and is only
# valid on the target that produces the engine DLL (BUILD-F02 invariant I4).
if(MINENGINE_ENABLE_EXTRA_TESTS)
    target_compile_definitions(${MINENGINE_TESTS_NAME} PRIVATE MINENGINE_ENABLE_EXTRA_TESTS=1)
endif()

target_link_libraries(${MINENGINE_TESTS_NAME} PRIVATE ${ENGINE_NAME})
# BUILD-F04 S04: the tests compile a curated subset of Editor/src/** (DebugCommand, PropertyPath,
# sprite authoring, automation pipe), and those headers include imgui.h. They used to inherit
# the imgui include dirs from the engine target's PUBLIC list; imgui belongs to the editor DLL
# now, so the header interface is linked explicitly. Header-only: the tests reference no imgui
# symbol (0 imgui imports in the pre-S04 baseline and after).
target_link_libraries(${MINENGINE_TESTS_NAME} PRIVATE minengine_imgui_headers)
# BUILD-F04 S04: the engine no longer defines glfw symbols (glfw is a shared DLL now), and some
# suites call glfwInit/glfwCreateWindow/glfwLoadGLLoader directly, so the tests link glfw3.dll
# themselves - the same step BUILD-F03 slice 1 planned for them.
target_link_libraries(${MINENGINE_TESTS_NAME} PRIVATE minengine_glfw)
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
add_dependencies(${MINENGINE_TESTS_NAME} ${MINENGINE_CRASH_PROBE_NAME})
add_dependencies(${MINENGINE_TESTS_NAME} ${MINENGINE_CONTRACT_PROBE_NAME})
