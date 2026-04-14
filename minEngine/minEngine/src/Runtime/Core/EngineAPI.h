#pragma once

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)   
    #ifdef MINENGINE_BUILD
        #define MINENGINE_API   
        /*__declspec(dllexport)*/     
    #else
        #define MINENGINE_API 
        /*__declspec(dllimport)*/
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define MINENGINE_API __attribute__ ((visibility ("default")))
#else
    #define MINENGINE_API
#endif