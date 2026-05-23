#pragma once

#include <cstdio>

#ifndef ME_ASSERT_ENABLED
    #define ME_ASSERT_ENABLED 1
#endif

// Debug-build assert: log message to stderr, then break (attach debugger) or trap.
#if ME_ASSERT_ENABLED
    #define ME_ASSERT(expr, message) \
        do { \
            if (!(expr)) { \
                std::fprintf( \
                    stderr, \
                    "ME_ASSERT failed at %s:%d: %s\n", \
                    __FILE__, \
                    __LINE__, \
                    (message)); \
                __debugbreak(); \
            } \
        } while (0)
#else
    #define ME_ASSERT(expr, message) \
        do { \
        } while (0)
#endif