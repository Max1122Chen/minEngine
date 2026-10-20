# BUILD-F02 - three-state PE export machine for the engine DLL.
#
#   auto       legacy behaviour: --export-all-symbols (every symbol exported).
#              PE ordinals are 16-bit, so this mode is what got us to 65533/65535.
#   whitelist  --exclude-all-symbols (fail-closed) + cmake/minEngine_exports.def.
#              The .def is GENERATED from the consumers' PE import tables; it is a
#              contract, not a dump of the current export table.
#   explicit   --exclude-all-symbols only; MINENGINE_API (__declspec(dllexport)) is the
#              single source of truth. Final state, not enabled by this slice.
#
# Rollback is one configure line:  -DMINENGINE_EXPORT_MODE=auto
#
# Invariant: MINENGINE_BUILD must only be defined on the target that produces the engine
# DLL. Defining it on a consumer makes that consumer compile engine symbols as
# dllexport instead of dllimport.

set(MINENGINE_EXPORT_MODE "whitelist" CACHE STRING
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

function(minengine_apply_export_mode target)
    if(NOT WIN32)
        return()
    endif()
    if(NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
        # MSVC/llvm-msvc keeps its own default export behaviour (design Deferred D8).
        message(STATUS "minEngine exports: MSVC toolchain, MINENGINE_EXPORT_MODE ignored")
        return()
    endif()

    set(_mode "${MINENGINE_EXPORT_MODE}")
    if(NOT _mode MATCHES "^(auto|whitelist|explicit)$")
        message(FATAL_ERROR
            "MINENGINE_EXPORT_MODE='${_mode}' is invalid; use auto|whitelist|explicit")
    endif()

    # Kept verbatim from the pre-BUILD-F02 link line so that auto mode is byte-identical.
    # In whitelist/explicit mode they are redundant (nothing is auto-exported) but harmless.
    # Assimp/zlib entries cover the static-subproject fallback (no prebuilt DLL in bin/).
    set(_exclude_libs "LINKER:--exclude-libs=libJolt.a:libJoltd.a:libfreetype.a:libfreetyped.a:libminengine_lua.a:libminengine_luad.a:libnfd.a:libnfdd.a:libassimp.a:libassimpd.a:libzlibstatic.a:libzlibstaticd.a")
    set(_exclude_implib "LINKER:--exclude-modules-for-implib=miniaudio.c.obj")

    if(_mode STREQUAL "auto")
        target_link_options(${target} PRIVATE
            "LINKER:--export-all-symbols"
            "${_exclude_libs}"
            "${_exclude_implib}")
        message(STATUS "minEngine exports: mode=auto (--export-all-symbols; ordinal bomb ARMED)")
        return()
    endif()

    # fail-closed: without it ld falls back to auto-exporting every symbol (experiment V0).
    target_link_options(${target} PRIVATE
        "LINKER:--exclude-all-symbols"
        "${_exclude_libs}"
        "${_exclude_implib}")

    if(_mode STREQUAL "whitelist")
        if(NOT EXISTS "${MINENGINE_EXPORTS_DEF}")
            message(FATAL_ERROR "MINENGINE_EXPORTS_DEF not found: ${MINENGINE_EXPORTS_DEF}")
        endif()
        # A .def passed as a source reaches the MinGW link line as a plain input file
        # (experiment V2) and makes the DLL relink whenever the whitelist changes.
        target_sources(${target} PRIVATE "${MINENGINE_EXPORTS_DEF}")
        file(STRINGS "${MINENGINE_EXPORTS_DEF}" _def_lines)
        set(_entries 0)
        foreach(_line IN LISTS _def_lines)
            if(_line MATCHES "^[^;]" AND NOT _line STREQUAL "EXPORTS")
                math(EXPR _entries "${_entries} + 1")
            endif()
        endforeach()
        message(STATUS "minEngine exports: mode=whitelist, ${_entries} .def entries (${MINENGINE_EXPORTS_DEF})")
    else()
        message(STATUS "minEngine exports: mode=explicit (MINENGINE_API only)")
    endif()

    # O1/O2 guard: fail the link step, not the next product build, when the export table
    # grows past the budget (failure mode F7 destroys the artifact after the fact).
    if(EXISTS "${MINENGINE_AUDIT_SCRIPT}" AND Python3_EXECUTABLE)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${Python3_EXECUTABLE} "${MINENGINE_AUDIT_SCRIPT}"
                    --dll "$<TARGET_FILE:${target}>" --fail
                    --max-exports ${MINENGINE_EXPORT_BUDGET}
                    --min-headroom ${MINENGINE_EXPORT_MIN_HEADROOM}
            COMMENT "minEngine export budget (${_mode}): assert entry count and ordinal headroom"
            VERBATIM)
    else()
        message(WARNING "minEngine exports: audit script or Python3 missing, export budget NOT asserted")
    endif()
endfunction()
