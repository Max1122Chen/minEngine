cmake_minimum_required(VERSION 3.10)

project(minEngine)

set(CMAKE_CXX_STANDARD 17)

set(ENGINE_NAME minEngine)

# set path
# Third-Party libraries
set(THIRD_PARTY_PATH ${PROJECT_SOURCE_DIR}/${ENGINE_NAME}/Third-Party)


# Output paths
set(EXECUTABLE_PATH ${PROJECT_SOURCE_DIR}/bin)


add_subdirectory(${ENGINE_NAME})


