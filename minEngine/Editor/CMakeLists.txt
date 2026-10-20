# MaximumEditorLib (SHARED, BUILD-F04 S02) + thin MaximumEditor.exe.
# Project/Sample editors reuse the same DLL via minengine_add_editor_executable.

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

# BUILD-F04 S04: the imgui family moved here from the engine target. The ownership rule that
# decided it: the engine src has 0 ImGui:: calls, 0 GImGui references and 0 imgui_internal.h
# includes (its only trace was a dead 3-line include in Render/RenderSystem.cpp), while every
# one of the 253 imgui exports was consumed by editor hosts only. imgui never crossed a DLL
# boundary in a useful direction - it was compiled into the engine DLL purely to be re-exported.
# The demo TU stays excluded: it is not part of the editor UI surface either.
file(GLOB IMGUI ${THIRD_PARTY_PATH}/imgui/*.cpp
                ${THIRD_PARTY_PATH}/imgui/backends/imgui_impl_glfw.cpp
                ${THIRD_PARTY_PATH}/imgui/backends/imgui_impl_opengl3.cpp
)
list(FILTER IMGUI EXCLUDE REGEX ".*/imgui_demo\\.cpp$")
set(IMGUI_UPSTREAM_DIR ${THIRD_PARTY_PATH}/imgui-upstream)
set(IMGUI_FREETYPE_SRC ${IMGUI_UPSTREAM_DIR}/misc/freetype/imgui_freetype.cpp)
file(GLOB IMGUIZMO ${THIRD_PARTY_PATH}/ImGuizmo/*.cpp)

# BUILD-F04 S02: STATIC -> SHARED. Every editor host links ONE copy of the editor instead of
# compiling the same 152 TUs into each of them (design P1). The target name is kept (Q5) so
# minengine_add_editor_executable() and the external repos need no change; only the artifact
# gains the DLL form: libMaximumEditord.dll (release: libMaximumEditor.dll).
# Rollback (S02): change this one word back to STATIC.
add_library(MaximumEditorLib SHARED
    ${EDITOR_LIB_SRC}
    ${IMGUI_NODE_EDITOR_SRC}
    ${IMGUI}
    ${IMGUI_FREETYPE_SRC}
    ${IMGUIZMO}
)

set_target_properties(MaximumEditorLib PROPERTIES
    OUTPUT_NAME MaximumEditor
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
    LIBRARY_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
    ARCHIVE_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
)

# Invariant I-buildexact (Editor/src/EditorAPI.h): MINENGINE_EDITOR_BUILD may be defined ONLY on
# the target that produces the editor library. A consumer defining it would compile editor symbols
# as dllexport instead of dllimport.
target_compile_definitions(MaximumEditorLib PRIVATE MINENGINE_EDITOR_BUILD)

# BUILD-F04 S04: the imgui include dirs come from minengine_imgui_headers (CMakeLists.txt at the
# engine root), not from the engine target. The engine no longer advertises imgui headers at
# all, which is what "the engine contains no imgui" means in practice for a #include.
target_include_directories(MaximumEditorLib PUBLIC
    ./src
    ../minEngine
    ../minEngine/src
    ${IMGUI_NODE_EDITOR_DIR}
)

target_link_libraries(MaximumEditorLib PUBLIC ${ENGINE_NAME})

# BUILD-F04 S04 / design 4.5 + Q7 ①: FreeType gets a SECOND static instance here. The engine
# keeps its own copy for FontGlyphAtlas (Runtime/Function/UI/Font/FontGlyphAtlas.cpp) and
# imgui_freetype.cpp cannot compile without one, so the accepted cost is ~1-2 MB of duplicated
# code and two independent FT_Library objects. FreeType has no cross-instance global contract,
# which is exactly why this is safe here and would not be for imgui.
target_link_libraries(MaximumEditorLib PRIVATE freetype)

# BUILD-F04 S04: imgui_impl_glfw.cpp calls ~48 glfw functions. It links the SAME prebuilt
# glfw3.dll the engine imports - not a second static libglfw3.a, which would give the process
# two GLFW states (window, callbacks, input) and break input routing (design F7).
target_link_libraries(MaximumEditorLib PRIVATE minengine_glfw)

# BUILD-F04 S04 / design 4.6: PUBLIC, not PRIVATE. imgui's own TUs, imgui_freetype.cpp and the
# imgui-node-editor TUs all include imgui_internal.h; before this slice only the engine had the
# define, so node-editor compiled with IMGUI_ENABLE_STB_TRUETYPE while imgui core did not.
# The define now travels on the header interface, and linking it PUBLIC here is what makes it
# PUBLIC for this target (and for any host that ever includes imgui.h through an editor header).
target_link_libraries(MaximumEditorLib PUBLIC minengine_imgui_headers)

# BUILD-F04 S04 / design 4.7: the Vulkan backend follows imgui here. The engine resolved the
# package in ITS directory scope, so resolve it again in this one to get Vulkan::Vulkan locally
# (FindVulkan is idempotent) - otherwise a --rhi vulkan build would silently lose the backend.
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    target_sources(MaximumEditorLib PRIVATE
        ${THIRD_PARTY_PATH}/imgui/backends/imgui_impl_vulkan.cpp)
    target_link_libraries(MaximumEditorLib PRIVATE Vulkan::Vulkan)
else()
    message(WARNING "BUILD-F04 S04: Vulkan not found - imgui_impl_vulkan.cpp is not built ("
        "--rhi vulkan will have no imgui backend). The engine target has the same condition.")
endif()

# Everything that exists because of the DLL boundary is derived from the library type, so the
# rollback to the pre-S02 geometry really is the single word above: with STATIC there is no export
# table to police and no dllimport to resolve.
get_target_property(_editor_lib_type MaximumEditorLib TYPE)
if(_editor_lib_type STREQUAL "SHARED_LIBRARY")
    target_compile_definitions(MaximumEditorLib PRIVATE MINENGINE_EDITOR_DLL)

    # Same three-state machine as the engine DLL, with its own whitelist and budget so an
    # editor-only change can never widen the engine export contract (BUILD-F04 S02/S03, design §9).
    minengine_apply_export_mode(MaximumEditorLib
        MODE "${MINENGINE_EDITOR_EXPORT_MODE}"
        DEF "${MINENGINE_EDITOR_EXPORTS_DEF}"
        BUDGET "${MINENGINE_EDITOR_EXPORT_BUDGET}"
        MIN_HEADROOM "${MINENGINE_EDITOR_EXPORT_MIN_HEADROOM}"
        LABEL "MaximumEditorLib"
    )
endif()

set(EFSW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/Third-Party/efsw)
if(EXISTS ${EFSW_DIR}/CMakeLists.txt)
    # BUILD-F04 S02: plain variable, not CACHE ... FORCE. It is inherited by the efsw
    # subdirectory below and no longer leaks into every later add_subdirectory.
    set(BUILD_SHARED_LIBS OFF)
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
