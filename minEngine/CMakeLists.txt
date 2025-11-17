cmake_minimum_required(VERSION 3.10)

project(minEngine)

set(CMAKE_CXX_STANDARD 17)

set(ENGINE_NAME Engine)
set(CLIENT_NAME SandBox)

# set path
set(HEADER_PATH ${PROJECT_SOURCE_DIR}/${ENGINE_NAME}/src)
set(EXECUTABLE_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIB_PATH ${PROJECT_SOURCE_DIR}/bin)


add_subdirectory(${ENGINE_NAME})
add_subdirectory(${CLIENT_NAME})
