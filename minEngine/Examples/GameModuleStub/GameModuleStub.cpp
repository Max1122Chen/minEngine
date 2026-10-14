// Sample module entry + touch reflected types so aggressive LTO/GC keeps registration TUs.

#include "StubGameInstanceSubsystem.h"
#include "StubGamePrimitiveComponent.h"
#include "StubTickableWorldSubsystem.h"

#if defined(_WIN32) && defined(GAMEMODULESTUB_BUILD)
#    define GAMEMODULESTUB_API __declspec(dllexport)
#elif defined(_WIN32)
#    define GAMEMODULESTUB_API __declspec(dllimport)
#else
#    define GAMEMODULESTUB_API __attribute__((visibility("default")))
#endif

extern "C" GAMEMODULESTUB_API int MinEngine_GameModuleStub_Marker(void)
{
    (void)sizeof(GameModuleStub::StubGamePrimitiveComponent);
    (void)sizeof(GameModuleStub::StubGameInstanceSubsystem);
    (void)sizeof(GameModuleStub::StubTickableWorldSubsystem);
    return 1;
}
