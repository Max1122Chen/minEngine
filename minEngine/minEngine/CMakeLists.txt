file(GLOB_RECURSE SRC CONFIGURE_DEPENDS ./src/*.cpp)

file(GLOB IMGUI ${THIRD_PARTY_PATH}/imgui/*.cpp
                ${THIRD_PARTY_PATH}/imgui/backends/*.cpp
)

add_library(${ENGINE_NAME} SHARED ${SRC}
                                  ${THIRD_PARTY_PATH}/glad/src/glad.c
                                  ${THIRD_PARTY_PATH}/stb_image/stb_image.cpp
                                  ${IMGUI}
                                  )

set_target_properties(${ENGINE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
    LIBRARY_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
    ARCHIVE_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
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
                                          PUBLIC ${THIRD_PARTY_PATH}/imgui
                                          )

target_link_libraries(${ENGINE_NAME} PRIVATE glfw3 opengl32 assimp-6)

target_link_directories(${ENGINE_NAME} PUBLIC ${THIRD_PARTY_PATH}/glfw/lib-mingw-w64 
                                       PUBLIC ${THIRD_PARTY_PATH}/assimp/libs
                                       )