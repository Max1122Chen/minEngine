# MaximumEditorLib (STATIC) + thin MaximumEditor.exe.
# Project/Sample editors reuse the lib via minengine_add_editor_executable.

include("${MINENGINE_CMAKE_DIR}/MinEngineTargets.cmake")

file(GLOB_RECURSE EDITOR_LIB_SRC CONFIGURE_DEPENDS ./src/*.cpp)
list(FILTER EDITOR_LIB_SRC EXCLUDE REGEX ".*/EditorMain\\.cpp$")

set(IMGUI_NODE_EDITOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/Third-Party/imgui-node-editor)
set(IMGUI_NODE_EDITOR_SRC
    ${IMGUI_NODE_EDITOR_DIR}/imgui_node_editor.cpp
    ${IMGUI_NODE_EDITOR_DIR}/imgui_node_editor_api.cpp
    ${IMGUI_NODE_EDITOR_DIR}/imgui_canvas.cpp
    ${IMGUI_NODE_EDITOR_DIR}/crude_json.cpp
)

add_library(MaximumEditorLib STATIC
    ${EDITOR_LIB_SRC}
    ${IMGUI_NODE_EDITOR_SRC}
)

target_include_directories(MaximumEditorLib PUBLIC
    ./src
    ../minEngine
    ../minEngine/src
    ${IMGUI_NODE_EDITOR_DIR}
    ../minEngine/Third-Party/imgui
)

target_link_libraries(MaximumEditorLib PUBLIC ${ENGINE_NAME})

set(EFSW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/Third-Party/efsw)
if(EXISTS ${EFSW_DIR}/CMakeLists.txt)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TEST_APP OFF CACHE BOOL "" FORCE)
    set(EFSW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(${EFSW_DIR} ${CMAKE_BINARY_DIR}/third-party/efsw EXCLUDE_FROM_ALL)
    target_link_libraries(MaximumEditorLib PUBLIC efsw-static)
    target_include_directories(MaximumEditorLib PUBLIC ${EFSW_DIR}/include)
endif()

# CMake target name stays "Editor" (--target Editor). Artifact: MaximumEditor.exe
minengine_add_editor_executable(Editor
    MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/EditorMain.cpp"
    OUTPUT_NAME MaximumEditor
)
