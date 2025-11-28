aux_source_directory(./src SRC)
aux_source_directory(./src/Runtime RUNTIME)
aux_source_directory(./src/Runtime/Core CORE)
aux_source_directory(./src/Runtime/Core/Log LOG)
aux_source_directory(./src/Runtime/Function FUNCTION)
aux_source_directory(./src/Runtime/Function/Input INPUT)
aux_source_directory(./src/Runtime/Function/Render RENDER)
aux_source_directory(./src/Runtime/Function/Render/OpenGL OPENGL_API)


set(EXECUTABLE_OUTPUT_PATH ${EXECUTABLE_PATH})

add_executable(${ENGINE_NAME} ${SRC}
                              ${CORE} 
                              ${LOG} 
                              ${INPUT}
                              ${FUNCTION} 
                              ${RENDER}
                              ${OPENGL_API}
                              ${RUNTIME}
                              ${THIRD_PARTY_PATH}/glad/src/glad.c
                              ${THIRD_PARTY_PATH}/stb_image/stb_image.cpp
                              )


target_include_directories(${ENGINE_NAME} PUBLIC ./src
                                          PUBLIC ./src/Runtime
                                          PUBLIC ./src/Runtime/Core
                                          PUBLIC ./src/Runtime/Function
                                          PUBLIC ${THIRD_PARTY_PATH}
                                          PUBLIC ${THIRD_PARTY_PATH}/spdlog/include
                                          PUBLIC ${THIRD_PARTY_PATH}/glfw/include
                                          PUBLIC ${THIRD_PARTY_PATH}/glad/include
                                          PUBLIC ${THIRD_PARTY_PATH}/stb_image
                                          PUBLIC ${THIRD_PARTY_PATH}/assimp/include
                                          )

target_link_libraries(${ENGINE_NAME} PRIVATE glfw3 opengl32 assimp-6)

target_link_directories(${ENGINE_NAME} PUBLIC ${THIRD_PARTY_PATH}/glfw/lib-mingw-w64 
                                       PUBLIC ${THIRD_PARTY_PATH}/assimp/libs
                                       )