cmake_minimum_required(VERSION 3.20)

project(minEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options for application targets.
option(BUILD_PLAYGROUND "Build the Playground game target" OFF)
option(BUILD_EDITOR "Build the Editor target" ON)

set(ENGINE_NAME minEngine)
set(THIRD_PARTY_PATH ${PROJECT_SOURCE_DIR}/${ENGINE_NAME}/Third-Party)
set(EXECUTABLE_PATH ${PROJECT_SOURCE_DIR}/bin)

add_subdirectory(${ENGINE_NAME})
 

if(BUILD_EDITOR) 
	add_subdirectory(Editor)
endif()

option(BUILD_TESTS "Build minEngineTests headless test executable" ON)
if(BUILD_TESTS)
    add_subdirectory(Tests)
endif()


