#pragma once

#ifndef ME_ASSERT_ENABLED
    #define ME_ASSERT_ENABLED 1
#endif

// A simple assert macro for debug builds
#if ME_ASSERT_ENABLED
    #define ME_ASSERT(expr, message) \
        if (!(expr))                        \
        {                                   \
            __debugbreak();                 \
        }
#else   // ME_ASSERT_ENABLED == 0, release build
    #define ME_ASSERT(expr, message)
#endif