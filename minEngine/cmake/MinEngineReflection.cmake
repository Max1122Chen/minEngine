# minengine_add_reflection_codegen — reusable header-tool invocation (CORE-F27).
# Engine and external game SHARED targets each call this with their own SRC_ROOT / OUT_DIR.
#
# Usage:
#   include("${MINENGINE_CMAKE_DIR}/MinEngineReflection.cmake")
#   minengine_add_reflection_codegen(<target>
#     SRC_ROOT <abs-path>
#     OUT_DIR <abs-path>
#     [MANIFEST <path>]                 # default: OUT_DIR/manifest.json
#     [CODEGEN_TARGET_NAME <name>]      # default: <target>_reflection_codegen
#     [WORKING_DIRECTORY <path>]
#     [SCRIPT_BINDING_OUT_DIR <path>]
#     [SCRIPT_BINDING_MANIFEST <path>]
#     [MODULE_API <macro>]              # optional: defines ME_REFLECTION_MODULE_API=<macro>
#                                       # for this target (default: MINENGINE_API via headers)
#     [EXCLUDE_REGEX <regex> ...]
#     [ADD_GENERATED_SOURCES]           # ON by default; set OFF if caller already GLOBs gen.cpp
#   )

function(minengine_add_reflection_codegen TARGET_NAME)
    set(options ADD_GENERATED_SOURCES_OFF)
    set(oneValueArgs
        SRC_ROOT
        OUT_DIR
        MANIFEST
        CODEGEN_TARGET_NAME
        WORKING_DIRECTORY
        SCRIPT_BINDING_OUT_DIR
        SCRIPT_BINDING_MANIFEST
        GENERATOR_SCRIPT
        MODULE_API
    )
    set(multiValueArgs EXCLUDE_REGEX)
    cmake_parse_arguments(MERC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT TARGET "${TARGET_NAME}")
        message(FATAL_ERROR "minengine_add_reflection_codegen: target '${TARGET_NAME}' does not exist")
    endif()
    if("${MERC_SRC_ROOT}" STREQUAL "")
        message(FATAL_ERROR "minengine_add_reflection_codegen: SRC_ROOT is required")
    endif()
    if("${MERC_OUT_DIR}" STREQUAL "")
        message(FATAL_ERROR "minengine_add_reflection_codegen: OUT_DIR is required")
    endif()

    if(NOT Python3_EXECUTABLE)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
    endif()

    if("${MERC_GENERATOR_SCRIPT}" STREQUAL "")
        if(DEFINED MINENGINE_HEADER_TOOL)
            set(MERC_GENERATOR_SCRIPT "${MINENGINE_HEADER_TOOL}")
        else()
            message(FATAL_ERROR
                "minengine_add_reflection_codegen: set GENERATOR_SCRIPT or MINENGINE_HEADER_TOOL "
                "to scripts/minEngine_header_tool.py")
        endif()
    endif()

    if("${MERC_MANIFEST}" STREQUAL "")
        set(MERC_MANIFEST "${MERC_OUT_DIR}/manifest.json")
    endif()
    if("${MERC_CODEGEN_TARGET_NAME}" STREQUAL "")
        set(MERC_CODEGEN_TARGET_NAME "${TARGET_NAME}_reflection_codegen")
    endif()
    if("${MERC_WORKING_DIRECTORY}" STREQUAL "")
        set(MERC_WORKING_DIRECTORY "${MERC_SRC_ROOT}")
    endif()

    file(GLOB_RECURSE _merc_headers CONFIGURE_DEPENDS
        "${MERC_SRC_ROOT}/*.h"
        "${MERC_SRC_ROOT}/*.hpp"
    )
    foreach(_regex IN LISTS MERC_EXCLUDE_REGEX)
        list(FILTER _merc_headers EXCLUDE REGEX "${_regex}")
    endforeach()

    set(_merc_outputs "${MERC_MANIFEST}")
    set(_merc_cmd
        ${Python3_EXECUTABLE} "${MERC_GENERATOR_SCRIPT}"
        --src-root "${MERC_SRC_ROOT}"
        --out-dir "${MERC_OUT_DIR}"
        --manifest "${MERC_MANIFEST}"
    )

    if(NOT "${MERC_SCRIPT_BINDING_OUT_DIR}" STREQUAL "")
        if("${MERC_SCRIPT_BINDING_MANIFEST}" STREQUAL "")
            set(MERC_SCRIPT_BINDING_MANIFEST "${MERC_SCRIPT_BINDING_OUT_DIR}/manifest.json")
        endif()
        list(APPEND _merc_outputs "${MERC_SCRIPT_BINDING_MANIFEST}")
        list(APPEND _merc_cmd
            --script-binding-out-dir "${MERC_SCRIPT_BINDING_OUT_DIR}"
            --script-binding-manifest "${MERC_SCRIPT_BINDING_MANIFEST}"
        )
    endif()

    add_custom_command(
        OUTPUT ${_merc_outputs}
        COMMAND ${_merc_cmd}
        DEPENDS "${MERC_GENERATOR_SCRIPT}" ${_merc_headers}
        WORKING_DIRECTORY "${MERC_WORKING_DIRECTORY}"
        COMMENT "Generating reflection sources for ${TARGET_NAME}"
        VERBATIM
    )

    add_custom_target(${MERC_CODEGEN_TARGET_NAME} DEPENDS ${_merc_outputs})
    add_dependencies(${TARGET_NAME} ${MERC_CODEGEN_TARGET_NAME})

    if(NOT "${MERC_MODULE_API}" STREQUAL "")
        # Ownership of StaticClass / GetClassName follows the module that owns the .gen.cpp.
        target_compile_definitions(${TARGET_NAME} PRIVATE
            "ME_REFLECTION_MODULE_API=${MERC_MODULE_API}")
    endif()

    target_include_directories(${TARGET_NAME} PUBLIC "${MERC_OUT_DIR}")
    if(NOT "${MERC_SCRIPT_BINDING_OUT_DIR}" STREQUAL "")
        target_include_directories(${TARGET_NAME} PUBLIC "${MERC_SCRIPT_BINDING_OUT_DIR}")
    endif()

    if(NOT MERC_ADD_GENERATED_SOURCES_OFF)
        file(GLOB_RECURSE _merc_gen_cpp CONFIGURE_DEPENDS "${MERC_OUT_DIR}/*.gen.cpp")
        if(NOT "${MERC_SCRIPT_BINDING_OUT_DIR}" STREQUAL "")
            file(GLOB_RECURSE _merc_bind_cpp CONFIGURE_DEPENDS "${MERC_SCRIPT_BINDING_OUT_DIR}/*.gen.cpp")
            list(APPEND _merc_gen_cpp ${_merc_bind_cpp})
        endif()
        if(_merc_gen_cpp)
            target_sources(${TARGET_NAME} PRIVATE ${_merc_gen_cpp})
        endif()
    endif()
endfunction()
