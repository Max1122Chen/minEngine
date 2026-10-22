#pragma once

// Export surface for this sample game SHARED. Reflection StaticClass / GetClassName
// follow ME_REFLECTION_MODULE_API, which is aliased to this macro below.
#if defined(_WIN32) && defined(GAMEMODULESTUB_BUILD)
#    define GAMEMODULESTUB_API __declspec(dllexport)
#elif defined(_WIN32)
#    define GAMEMODULESTUB_API __declspec(dllimport)
#else
#    define GAMEMODULESTUB_API __attribute__((visibility("default")))
#endif

#ifndef ME_REFLECTION_MODULE_API
#    define ME_REFLECTION_MODULE_API GAMEMODULESTUB_API
#endif
