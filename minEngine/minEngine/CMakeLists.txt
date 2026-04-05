file(GLOB_RECURSE SRC CONFIGURE_DEPENDS ./src/*.cpp)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(REFLECTION_GENERATOR_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/minEngine_header_tool.py)
set(REFLECTION_SCAN_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(REFLECTION_GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/Generated/Reflection)
set(REFLECTION_GENERATED_MANIFEST ${REFLECTION_GENERATED_DIR}/manifest.json)
file(GLOB_RECURSE REFLECTION_SCAN_HEADERS CONFIGURE_DEPENDS
    ${REFLECTION_SCAN_ROOT}/*.h
    ${REFLECTION_SCAN_ROOT}/*.hpp
)

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

add_library(${ENGINE_NAME} SHARED ${SRC}
                                  ${THIRD_PARTY_PATH}/glad/src/glad.c
                                  ${THIRD_PARTY_PATH}/stb_image/stb_image.cpp
                                  ${IMGUI}
                                  )

add_dependencies(${ENGINE_NAME} reflection_codegen)

set_target_properties(${ENGINE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
    LIBRARY_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
    ARCHIVE_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
)


target_include_directories(${ENGINE_NAME} PUBLIC ./src
                                          PUBLIC ./src/Runtime
                                          PUBLIC ./src/Runtime/Core
                                          PUBLIC ./src/Runtime/Function
                                          PUBLIC ${REFLECTION_GENERATED_DIR}
                                          PUBLIC ${THIRD_PARTY_PATH}
                                          PUBLIC ${THIRD_PARTY_PATH}/spdlog/include
                                          PUBLIC ${THIRD_PARTY_PATH}/glfw/include
                                          PUBLIC ${THIRD_PARTY_PATH}/glad/include
                                          PUBLIC ${THIRD_PARTY_PATH}/stb_image
                                          PUBLIC ${THIRD_PARTY_PATH}/assimp/include
                                          PUBLIC ${THIRD_PARTY_PATH}/imgui
                                          )

target_link_libraries(${ENGINE_NAME} PRIVATE glfw3 opengl32 assimp-6)

target_link_directories(${ENGINE_NAME} PUBLIC ${THIRD_PARTY_PATH}/glfw/lib-mingw-w64 
                                       PUBLIC ${THIRD_PARTY_PATH}/assimp/libs
                                       )