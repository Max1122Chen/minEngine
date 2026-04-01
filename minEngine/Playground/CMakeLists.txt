set(PLAYGROUND_NAME Playground)

aux_source_directory(./src SRC)

add_executable(${PLAYGROUND_NAME} ${SRC})

set_target_properties(${PLAYGROUND_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
)


target_include_directories(${PLAYGROUND_NAME} PUBLIC ./src
                                              PUBLIC ../minEngine
                                              PUBLIC ../minEngine/src
                                          )

target_link_libraries(${PLAYGROUND_NAME} PRIVATE ${ENGINE_NAME})
