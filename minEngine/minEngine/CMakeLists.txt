aux_source_directory(./src SRC)
aux_source_directory(./src/Runtime RUNTIME)
aux_source_directory(./src/Runtime/Core CORE)
aux_source_directory(./src/Runtime/Core/Log LOG)
aux_source_directory(./src/Runtime/Function FUNCTION)
aux_source_directory(./src/Runtime/Function/Render RENDER)


set(EXECUTABLE_OUTPUT_PATH ${EXECUTABLE_PATH})

add_executable(${ENGINE_NAME} ${SRC}
                              ${CORE} 
                              ${LOG} 
                              ${FUNCTION} 
                              ${RENDER} 
                              ${RUNTIME}
                              ${THIRD_PARTY_PATH}/glad/src/glad.c)


target_include_directories(${ENGINE_NAME} PUBLIC ./src
                                          PUBLIC ./src/Runtime
                                          PUBLIC ./src/Runtime/Core
                                          PUBLIC ./src/Runtime/Function
                                          PUBLIC ${THIRD_PARTY_PATH}/spdlog/include
                                          PUBLIC ${THIRD_PARTY_PATH}/glfw/include
                                          PUBLIC ${THIRD_PARTY_PATH}/glad/include
                                          PUBLIC ${THIRD_PARTY_PATH}/stb_image
                                          )

target_link_libraries(${ENGINE_NAME} PRIVATE glfw3 opengl32)

target_link_directories(${ENGINE_NAME} PUBLIC ${THIRD_PARTY_PATH}/glfw/lib-mingw-w64)