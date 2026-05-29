set(MINENGINE_TESTS_NAME minEngineTests)

file(GLOB TEST_SUITE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Suites/*.cpp
)

add_executable(${MINENGINE_TESTS_NAME}
    TestMain.cpp
    TestSuiteRegistration.cpp
    EngineTestFixture.cpp
    DoctestSuiteRunner.cpp
    ${TEST_SUITE_SOURCES}
)

set_target_properties(${MINENGINE_TESTS_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
)

target_include_directories(${MINENGINE_TESTS_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Suites
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

target_compile_definitions(${MINENGINE_TESTS_NAME} PRIVATE MINENGINE_BUILD)

target_link_libraries(${MINENGINE_TESTS_NAME} PRIVATE ${ENGINE_NAME})

add_dependencies(${MINENGINE_TESTS_NAME} ${ENGINE_NAME})
