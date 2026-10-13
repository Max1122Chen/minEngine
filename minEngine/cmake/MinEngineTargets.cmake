# Helpers for product Editor / Game host executables.
# Include after the minEngine target exists.

include_guard(GLOBAL)

# minengine_add_game_executable(<name>
#   SOURCES <files...>
#   [GAME_MODULE <target>]
#   [OUTPUT_NAME <name>]
# )
function(minengine_add_game_executable TARGET_NAME)
    cmake_parse_arguments(ARG "" "GAME_MODULE;OUTPUT_NAME" "SOURCES" ${ARGN})
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "minengine_add_game_executable(${TARGET_NAME}): SOURCES required")
    endif()
    if(NOT TARGET minEngine)
        message(FATAL_ERROR "minengine_add_game_executable(${TARGET_NAME}): target minEngine missing")
    endif()
    if(NOT DEFINED EXECUTABLE_PATH OR "${EXECUTABLE_PATH}" STREQUAL "")
        if(DEFINED MINENGINE_OUTPUT_DIR AND NOT "${MINENGINE_OUTPUT_DIR}" STREQUAL "")
            set(EXECUTABLE_PATH "${MINENGINE_OUTPUT_DIR}")
        else()
            message(FATAL_ERROR "minengine_add_game_executable(${TARGET_NAME}): EXECUTABLE_PATH unset")
        endif()
    endif()

    add_executable(${TARGET_NAME} ${ARG_SOURCES})
    set(_out_name "${TARGET_NAME}")
    if(ARG_OUTPUT_NAME)
        set(_out_name "${ARG_OUTPUT_NAME}")
    endif()
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_PATH}"
        OUTPUT_NAME "${_out_name}"
    )
    target_link_libraries(${TARGET_NAME} PRIVATE minEngine)
    if(ARG_GAME_MODULE)
        if(NOT TARGET "${ARG_GAME_MODULE}")
            message(FATAL_ERROR
                "minengine_add_game_executable(${TARGET_NAME}): GAME_MODULE '${ARG_GAME_MODULE}' not a target")
        endif()
        target_link_libraries(${TARGET_NAME} PRIVATE ${ARG_GAME_MODULE})
        add_dependencies(${TARGET_NAME} ${ARG_GAME_MODULE})
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${ARG_GAME_MODULE}>"
                "$<TARGET_FILE_DIR:${TARGET_NAME}>"
            COMMENT "Copy ${ARG_GAME_MODULE} next to ${_out_name}"
            VERBATIM
        )
    endif()
endfunction()

# minengine_add_editor_executable(<name>
#   MAIN_SOURCE <file>
#   [GAME_MODULE <target>]
#   [OUTPUT_NAME <name>]
# )
# Requires MaximumEditorLib (from Editor/CMakeLists.txt).
function(minengine_add_editor_executable TARGET_NAME)
    cmake_parse_arguments(ARG "" "GAME_MODULE;OUTPUT_NAME;MAIN_SOURCE" "" ${ARGN})
    if(NOT TARGET MaximumEditorLib)
        message(FATAL_ERROR
            "minengine_add_editor_executable(${TARGET_NAME}): MaximumEditorLib missing — build Editor first")
    endif()
    if(NOT ARG_MAIN_SOURCE)
        message(FATAL_ERROR "minengine_add_editor_executable(${TARGET_NAME}): MAIN_SOURCE required")
    endif()
    if(NOT DEFINED EXECUTABLE_PATH OR "${EXECUTABLE_PATH}" STREQUAL "")
        if(DEFINED MINENGINE_OUTPUT_DIR AND NOT "${MINENGINE_OUTPUT_DIR}" STREQUAL "")
            set(EXECUTABLE_PATH "${MINENGINE_OUTPUT_DIR}")
        else()
            message(FATAL_ERROR "minengine_add_editor_executable(${TARGET_NAME}): EXECUTABLE_PATH unset")
        endif()
    endif()

    add_executable(${TARGET_NAME} "${ARG_MAIN_SOURCE}")
    set(_out_name "${TARGET_NAME}")
    if(ARG_OUTPUT_NAME)
        set(_out_name "${ARG_OUTPUT_NAME}")
    endif()
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_PATH}"
        OUTPUT_NAME "${_out_name}"
    )
    target_link_libraries(${TARGET_NAME} PRIVATE MaximumEditorLib)
    if(ARG_GAME_MODULE)
        if(NOT TARGET "${ARG_GAME_MODULE}")
            message(FATAL_ERROR
                "minengine_add_editor_executable(${TARGET_NAME}): GAME_MODULE '${ARG_GAME_MODULE}' not a target")
        endif()
        target_link_libraries(${TARGET_NAME} PRIVATE ${ARG_GAME_MODULE})
        add_dependencies(${TARGET_NAME} ${ARG_GAME_MODULE})
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${ARG_GAME_MODULE}>"
                "$<TARGET_FILE_DIR:${TARGET_NAME}>"
            COMMENT "Copy ${ARG_GAME_MODULE} next to ${_out_name}"
            VERBATIM
        )
    endif()
endfunction()

# minengine_add_package_game(<exe_target>
#   STAGE_DIR <dir>
#   [CONTENT_DIR <dir>]
#   [RUNTIME_TARGETS <targets...>]
# )
# Custom target package-<exe_target>: copy exe + runtime (+ optional Content). No Cook.
function(minengine_add_package_game EXE_TARGET)
    cmake_parse_arguments(ARG "" "STAGE_DIR;CONTENT_DIR" "RUNTIME_TARGETS" ${ARGN})
    if(NOT TARGET "${EXE_TARGET}")
        message(FATAL_ERROR "minengine_add_package_game: target '${EXE_TARGET}' missing")
    endif()
    if(NOT ARG_STAGE_DIR)
        message(FATAL_ERROR "minengine_add_package_game(${EXE_TARGET}): STAGE_DIR required")
    endif()

    set(_pkg_target "package-${EXE_TARGET}")
    set(_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ARG_STAGE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${EXE_TARGET}>"
            "${ARG_STAGE_DIR}/"
    )
    foreach(_rt IN LISTS ARG_RUNTIME_TARGETS)
        if(NOT TARGET "${_rt}")
            message(FATAL_ERROR "minengine_add_package_game(${EXE_TARGET}): RUNTIME_TARGETS '${_rt}' missing")
        endif()
        list(APPEND _commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${_rt}>"
                "${ARG_STAGE_DIR}/"
        )
    endforeach()
    if(ARG_CONTENT_DIR)
        if(IS_DIRECTORY "${ARG_CONTENT_DIR}")
            list(APPEND _commands
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${ARG_CONTENT_DIR}"
                    "${ARG_STAGE_DIR}/Content"
            )
        endif()
    endif()

    add_custom_target(${_pkg_target}
        ${_commands}
        DEPENDS ${EXE_TARGET} ${ARG_RUNTIME_TARGETS}
        COMMENT "Stage ${EXE_TARGET} → ${ARG_STAGE_DIR}"
        VERBATIM
    )
endfunction()
