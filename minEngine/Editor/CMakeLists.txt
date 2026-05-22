set(EDITOR_NAME Editor)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS ./src/*.cpp)

set(IMGUI_NODE_EDITOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/Third-Party/imgui-node-editor)
set(IMGUI_NODE_EDITOR_SRC
    ${IMGUI_NODE_EDITOR_DIR}/imgui_node_editor.cpp
    ${IMGUI_NODE_EDITOR_DIR}/imgui_node_editor_api.cpp
    ${IMGUI_NODE_EDITOR_DIR}/imgui_canvas.cpp
    ${IMGUI_NODE_EDITOR_DIR}/crude_json.cpp
)

add_executable(${EDITOR_NAME} ${SRC} ${IMGUI_NODE_EDITOR_SRC})

set_target_properties(${EDITOR_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
)

target_include_directories(${EDITOR_NAME} PUBLIC ./src
                                          PUBLIC ../minEngine
                                          PUBLIC ../minEngine/src
                                          PUBLIC ${IMGUI_NODE_EDITOR_DIR}
                                          PUBLIC ../minEngine/Third-Party/imgui
)

target_link_libraries(${EDITOR_NAME} PRIVATE ${ENGINE_NAME})
