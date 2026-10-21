#pragma once

// EDITOR_API carries real __declspec(dllexport/dllimport) semantics on Windows,
// isomorphic to Runtime/Core/EngineAPI.h.
//
// Invariant I-buildexact: MINENGINE_EDITOR_BUILD is defined ONLY on the target that produces the
// editor library (MaximumEditorLib), never on a consumer - a consumer defining it would compile
// editor symbols as dllexport instead of dllimport.
// MINENGINE_EDITOR_DLL exists on both sides ONLY while that library is built SHARED. It is what
// makes the one-line rollback in minEngine/Editor/CMakeLists.txt (SHARED -> STATIC) link without
// touching a single source file: with no DLL boundary the macro collapses to nothing.
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #ifdef MINENGINE_EDITOR_BUILD
        #ifdef MINENGINE_EDITOR_DLL
            #define EDITOR_API __declspec(dllexport)
        #else
            #define EDITOR_API
        #endif
    #elif defined(MINENGINE_EDITOR_HOST_LINKS_EDITOR_DLL)
        #define EDITOR_API __declspec(dllimport)
    #else
        #define EDITOR_API
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define EDITOR_API __attribute__ ((visibility ("default")))
#else
    #define EDITOR_API
#endif