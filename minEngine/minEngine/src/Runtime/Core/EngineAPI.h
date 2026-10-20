#pragma once

// BUILD-F02: MINENGINE_API carries real __declspec(dllexport/dllimport) semantics on Windows.
// Invariant: MINENGINE_BUILD is defined ONLY on the target that produces the engine DLL
// (minEngine/minEngine/CMakeLists.txt). A consumer that defines it compiles engine symbols
// as dllexport instead of dllimport and fails at link time with "cannot export undefined symbol".
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #ifdef MINENGINE_BUILD
        #define MINENGINE_API __declspec(dllexport)
    #else
        #define MINENGINE_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define MINENGINE_API __attribute__ ((visibility ("default")))
#else
    #define MINENGINE_API
#endif
