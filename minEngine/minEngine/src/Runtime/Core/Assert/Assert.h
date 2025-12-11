#pragma once

#ifndef MINENGINE_ASSERT_ENABLED
    #define MINENGINE_ASSERT_ENABLED 1
#endif

// A simple assert macro for debug builds
#if MINENGINE_ASSERT_ENABLED
    #define MINENGINE_ASSERT(expr, message) \
        if (!(expr))                        \
        {                                   \
            __debugbreak();                 \
        }
#else   // MINENGINE_ASSERT_ENABLED == 0, release build
    #define MINENGINE_ASSERT(expr, message)
#endif