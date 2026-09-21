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

# CMake target stays "Editor" (--target Editor). Shipping artifact: MaximumEditor.exe
# (BUG-EDITOR-003: bare Maximum.exe hit host GPU/OS coupling on at least one machine).
set_target_properties(${EDITOR_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
    OUTPUT_NAME MaximumEditor
)

target_include_directories(${EDITOR_NAME} PUBLIC ./src
                                          PUBLIC ../minEngine
                                          PUBLIC ../minEngine/src
                                          PUBLIC ${IMGUI_NODE_EDITOR_DIR}
                                          PUBLIC ../minEngine/Third-Party/imgui
)

target_link_libraries(${EDITOR_NAME} PRIVATE ${ENGINE_NAME})

# ED-F20: optional link-time game SHARED (Dev host). Off unless root CMake enabled it.
if(MINENGINE_HAS_GAME_MODULE)
    target_link_libraries(${EDITOR_NAME} PRIVATE ${MINENGINE_GAME_TARGET})
    target_compile_definitions(${EDITOR_NAME} PRIVATE MINENGINE_HAS_GAME_MODULE=1)
    add_dependencies(${EDITOR_NAME} ${MINENGINE_GAME_TARGET})

    if("${MINENGINE_GAME_TARGET}" STREQUAL "GameModuleStub")
        target_compile_definitions(${EDITOR_NAME} PRIVATE MINENGINE_LINK_GAME_MODULE_STUB=1)
    endif()

    if(MINENGINE_GAME_COPY_RUNTIME)
        add_custom_command(TARGET ${EDITOR_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${MINENGINE_GAME_TARGET}>"
                "$<TARGET_FILE_DIR:${EDITOR_NAME}>"
            COMMENT "ED-F20: copy game module runtime next to MaximumEditor"
            VERBATIM
        )
    endif()
endif()

set(EFSW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/Third-Party/efsw)
if(EXISTS ${EFSW_DIR}/CMakeLists.txt)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TEST_APP OFF CACHE BOOL "" FORCE)
    set(EFSW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(${EFSW_DIR} ${CMAKE_BINARY_DIR}/third-party/efsw EXCLUDE_FROM_ALL)
    target_link_libraries(${EDITOR_NAME} PRIVATE efsw-static)
    target_include_directories(${EDITOR_NAME} PRIVATE ${EFSW_DIR}/include)
endif()
