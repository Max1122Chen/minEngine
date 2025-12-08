aux_source_directory(./src SRC)

set(EXECUTABLE_OUTPUT_PATH ${EXECUTABLE_PATH})

add_executable(${PLAYGROUND_NAME} ${SRC})


target_include_directories(${PLAYGROUND_NAME} PUBLIC ./src
                                              PUBLIC ../minEngine
                                          )

target_link_libraries(${PLAYGROUND_NAME} PRIVATE ${ENGINE_NAME})
