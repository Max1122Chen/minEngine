# BUILD-F02 - three-state PE export machine for the engine DLL.
#
#   auto       legacy behaviour: --export-all-symbols (every symbol exported).
#              PE ordinals are 16-bit, so this mode is what got us to 65533/65535.
#   whitelist  --exclude-all-symbols (fail-closed) + cmake/minEngine_exports.def.
#              The .def is GENERATED from the consumers' PE import tables; it is a
#              contract, not a dump of the current export table.
#   explicit   --exclude-all-symbols only; MINENGINE_API (__declspec(dllexport)) is the
#              single source of truth. Default after knife-2 batch 5 emptied the .def.
#
# Rollback is one configure line:  -DMINENGINE_EXPORT_MODE=whitelist  (or auto)
#
# Invariant: MINENGINE_BUILD must only be defined on the target that produces the engine
# DLL. Defining it on a consumer makes that consumer compile engine symbols as
# dllexport instead of dllimport.

set(MINENGINE_EXPORT_MODE "explicit" CACHE STRING
    "minEngine PE export mode: auto | whitelist | explicit")
set_property(CACHE MINENGINE_EXPORT_MODE PROPERTY STRINGS auto whitelist explicit)

set(MINENGINE_EXPORTS_DEF "${MINENGINE_CMAKE_DIR}/minEngine_exports.def" CACHE FILEPATH
    "Export whitelist (.def) used when MINENGINE_EXPORT_MODE=whitelist")

set(MINENGINE_EXPORT_BUDGET "3000" CACHE STRING
    "Fail the build when the engine DLL exports more PE entries than this")
set(MINENGINE_EXPORT_MIN_HEADROOM "60000" CACHE STRING
    "Fail the build when 65535-maxNamedOrdinal drops below this")

set(MINENGINE_AUDIT_SCRIPT "${PROJECT_SOURCE_DIR}/../scripts/audit_dll_exports.py" CACHE FILEPATH
    "scripts/audit_dll_exports.py (PE export/import audit)")

# BUILD-F03 C8/Q8: live consumer bins asserted against the whitelist after every link.
# Empty by default (a build must not depend on another repo's artifacts being present);
# enable per workspace: -DMINENGINE_CONSUMER_DIRS="../minCraft*/bin"
set(MINENGINE_CONSUMER_DIRS "" CACHE STRING
    "Directories of live consumer binaries whose engine imports must all be exported")

# BUILD-F04 S02/S03: the editor DLL runs the SAME state machine under its own mode, whitelist and
# budget, so an editor-only change can never widen the engine export contract (or vice versa).
# Rollback to the S02 geometry is two steps (see BUILD-F04_S01_S03_EDITOR_DLL_IMPLEMENTATION.md):
# regenerate cmake/MaximumEditor_exports.def from the consumer import tables, then pass
# -DMINENGINE_EDITOR_EXPORT_MODE=whitelist.
set(MINENGINE_EDITOR_EXPORT_MODE "explicit" CACHE STRING
    "Editor DLL PE export mode: auto | whitelist | explicit")
set_property(CACHE MINENGINE_EDITOR_EXPORT_MODE PROPERTY STRINGS auto whitelist explicit)

set(MINENGINE_EDITOR_EXPORTS_DEF "${MINENGINE_CMAKE_DIR}/MaximumEditor_exports.def" CACHE FILEPATH
    "Export whitelist (.def) used when MINENGINE_EDITOR_EXPORT_MODE=whitelist")

set(MINENGINE_EDITOR_EXPORT_BUDGET "8" CACHE STRING
    "Fail the build when the editor DLL exports more PE entries than this (O6: EDITOR_API only)")
set(MINENGINE_EDITOR_EXPORT_MIN_HEADROOM "60000" CACHE STRING
    "Fail the build when 65535-maxNamedOrdinal of the editor DLL drops below this")

# BUILD-F04 S02 finding: the post-build export assertion used the plain Python3_EXECUTABLE, which
# find_package() sets as a NORMAL variable in minEngine/CMakeLists.txt's directory scope. Sibling
# directories (Editor/) never inherit it, so the editor DLL got no budget assertion at all while
# the configure log still looked healthy - the exact false-green shape this module must not have.
# Resolve the interpreter once into a CACHE variable so every including module sees it.
if(NOT MINENGINE_PYTHON_EXECUTABLE)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_EXECUTABLE)
        set(MINENGINE_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}" CACHE FILEPATH
            "Python interpreter used for the post-build PE export assertions")
    endif()
endif()

# minengine_apply_export_mode(<target>
#   [MODE <auto|whitelist|explicit>]   default: MINENGINE_EXPORT_MODE
#   [DEF <path.def>]                   default: MINENGINE_EXPORTS_DEF
#   [BUDGET <n>]                       default: MINENGINE_EXPORT_BUDGET
#   [MIN_HEADROOM <n>]                 default: MINENGINE_EXPORT_MIN_HEADROOM
#   [LABEL <name>]                     default: the target name (used in messages)
# )
# BUILD-F04 S02 added the optional arguments so a second module (the editor DLL) reuses this one
# state machine with its own whitelist/budget instead of growing a parallel implementation.
function(minengine_apply_export_mode target)
    cmake_parse_arguments(EXPORT "" "MODE;DEF;BUDGET;MIN_HEADROOM;LABEL" "" ${ARGN})
    set(_label "${target}")
    if(EXPORT_LABEL)
        set(_label "${EXPORT_LABEL}")
    endif()
    if(NOT WIN32)
        return()
    endif()
    if(NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
        # MSVC/llvm-msvc keeps its own default export behaviour (design Deferred D8).
        message(STATUS "${_label} exports: MSVC toolchain, export mode ignored")
        return()
    endif()

    set(_mode "${MINENGINE_EXPORT_MODE}")
    set(_def "${MINENGINE_EXPORTS_DEF}")
    set(_budget "${MINENGINE_EXPORT_BUDGET}")
    set(_headroom "${MINENGINE_EXPORT_MIN_HEADROOM}")
    if(EXPORT_MODE)
        set(_mode "${EXPORT_MODE}")
    endif()
    if(EXPORT_DEF)
        set(_def "${EXPORT_DEF}")
    endif()
    if(EXPORT_BUDGET)
        set(_budget "${EXPORT_BUDGET}")
    endif()
    if(EXPORT_MIN_HEADROOM)
        set(_headroom "${EXPORT_MIN_HEADROOM}")
    endif()

    if(NOT _mode MATCHES "^(auto|whitelist|explicit)$")
        message(FATAL_ERROR
            "${_label} export mode '${_mode}' is invalid; use auto|whitelist|explicit")
    endif()

    # Assimp/zlib entries cover the static-subproject fallback (no prebuilt DLL in bin/).
    # Only needed in auto mode; whitelist/explicit already fail-closed via --exclude-all-symbols.
    set(_exclude_libs "LINKER:--exclude-libs=libJolt.a:libJoltd.a:libfreetype.a:libfreetyped.a:libminengine_lua.a:libminengine_luad.a:libnfd.a:libnfdd.a:libassimp.a:libassimpd.a:libzlibstatic.a:libzlibstaticd.a")
    set(_exclude_implib "LINKER:--exclude-modules-for-implib=miniaudio.c.obj")

    if(_mode STREQUAL "auto")
        target_link_options(${target} PRIVATE
            "LINKER:--export-all-symbols"
            "${_exclude_libs}"
            "${_exclude_implib}")
        message(STATUS "${_label} exports: mode=auto (--export-all-symbols; ordinal bomb ARMED)")
        return()
    endif()

    # fail-closed: without it ld falls back to auto-exporting every symbol (experiment V0).
    target_link_options(${target} PRIVATE
        "LINKER:--exclude-all-symbols")

    if(_mode STREQUAL "whitelist")
        if(NOT EXISTS "${_def}")
            message(FATAL_ERROR "${_label} export whitelist not found: ${_def}")
        endif()
        # A .def passed as a source reaches the MinGW link line as a plain input file
        # (experiment V2) and makes the DLL relink whenever the whitelist changes.
        target_sources(${target} PRIVATE "${_def}")
        # Count entries from the raw text: file(STRINGS) returns a CMake list, so a comment
        # containing ';' would be split into fragments and silently inflate the count.
        file(READ "${_def}" _def_text)
        string(REGEX REPLACE "\r" "" _def_text "\n${_def_text}")
        string(REGEX REPLACE "\nEXPORTS\n" "\n" _def_text "${_def_text}")
        string(REGEX MATCHALL "\n[^;\n][^\n]*" _def_entries "${_def_text}")
        list(LENGTH _def_entries _entries)
        message(STATUS "${_label} exports: mode=whitelist, ${_entries} .def entries (${_def})")
    else()
        message(STATUS "${_label} exports: mode=explicit (EDITOR_API / MINENGINE_API only)")
    endif()

    # O1/O2 guard: fail the link step, not the next product build, when the export table
    # grows past the budget (failure mode F7 destroys the artifact after the fact).
    if(EXISTS "${MINENGINE_AUDIT_SCRIPT}" AND MINENGINE_PYTHON_EXECUTABLE)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${MINENGINE_PYTHON_EXECUTABLE} "${MINENGINE_AUDIT_SCRIPT}"
                    --dll "$<TARGET_FILE:${target}>" --fail
                    --max-exports ${_budget}
                    --min-headroom ${_headroom}
            COMMENT "${_label} export budget (${_mode}): assert entry count and ordinal headroom"
            VERBATIM)
        if(MINENGINE_CONSUMER_DIRS)
            set(_consumer_args "")
            foreach(_consumer_dir IN LISTS MINENGINE_CONSUMER_DIRS)
                list(APPEND _consumer_args --consumer-dir "${_consumer_dir}")
            endforeach()
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${MINENGINE_PYTHON_EXECUTABLE} "${MINENGINE_AUDIT_SCRIPT}"
                        --dll "$<TARGET_FILE:${target}>" --verify-coverage ${_consumer_args}
                COMMENT "minEngine export contract: live consumer imports (${MINENGINE_CONSUMER_DIRS})"
                VERBATIM)
        endif()
    else()
        message(WARNING "${_label} exports: audit script or Python3 missing "
            "(${MINENGINE_AUDIT_SCRIPT}, '${MINENGINE_PYTHON_EXECUTABLE}') - export budget NOT asserted")
    endif()
endfunction()
