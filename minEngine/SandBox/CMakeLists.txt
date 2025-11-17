aux_source_directory(./src SRC)

set(EXECUTABLE_OUTPUT_PATH ${EXECUTABLE_PATH})

add_executable(${CLIENT_NAME} ${SRC})

target_include_directories(${CLIENT_NAME} PUBLIC ${HEADER_PATH})

target_link_directories(${CLIENT_NAME} PUBLIC ${LIB_PATH})
target_link_libraries(${CLIENT_NAME} PRIVATE ${ENGINE_NAME})


