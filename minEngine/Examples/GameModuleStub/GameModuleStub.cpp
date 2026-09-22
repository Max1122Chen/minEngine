// ED-F20 marker + CORE-F27 external ME_CLASS keep-alive.
// Load-time link keeps GameModuleStub.dll; .gen.cpp static registration runs before Finalize.

#include "StubGamePrimitiveComponent.h"

#if defined(_WIN32) && defined(GAMEMODULESTUB_BUILD)
#    define GAMEMODULESTUB_API __declspec(dllexport)
#elif defined(_WIN32)
#    define GAMEMODULESTUB_API __declspec(dllimport)
#else
#    define GAMEMODULESTUB_API __attribute__((visibility("default")))
#endif

extern "C" GAMEMODULESTUB_API int MinEngine_GameModuleStub_Marker(void)
{
    // Touch the reflected type so linkers keep registration TU when LTO/GC is aggressive.
    (void)sizeof(GameModuleStub::StubGamePrimitiveComponent);
    return 1;
}
