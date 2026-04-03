set(EDITOR_NAME Editor)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS ./src/*.cpp)

add_executable(${EDITOR_NAME} ${SRC})

set_target_properties(${EDITOR_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
)

target_include_directories(${EDITOR_NAME} PUBLIC ./src
                                          PUBLIC ../minEngine
                                          PUBLIC ../minEngine/src
)

target_link_libraries(${EDITOR_NAME} PRIVATE ${ENGINE_NAME})
