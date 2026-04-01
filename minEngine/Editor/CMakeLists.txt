set(EDITOR_NAME Editor)

aux_source_directory(./src SRC)

add_executable(${EDITOR_NAME} ${SRC})

set_target_properties(${EDITOR_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
)

target_include_directories(${EDITOR_NAME} PUBLIC ./src
                                          PUBLIC ../minEngine
                                          PUBLIC ../minEngine/src
)

target_link_libraries(${EDITOR_NAME} PRIVATE ${ENGINE_NAME})
