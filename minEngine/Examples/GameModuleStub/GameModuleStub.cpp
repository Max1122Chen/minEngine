// Sample module entry: fixed MinEngine_GameModuleEntry symbol + touch marker.
// reflected types so aggressive LTO/GC keeps registration TUs.

#include "Runtime/Core/Module/MinEngineGameModuleEntry.h"

#include "SampleTableDataAsset.h"
#include "StubGameInstance.h"
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

MINENGINE_IMPLEMENT_GAME_MODULE_ENTRY(GAMEMODULESTUB_API)

// Keep type TUs alive under LTO (optional; Entry alone is enough for force-load).
extern "C" GAMEMODULESTUB_API int MinEngine_GameModuleStub_TouchReflectedTypes(void)
{
    (void)sizeof(GameModuleStub::StubGamePrimitiveComponent);
    (void)sizeof(GameModuleStub::StubGameInstance);
    (void)sizeof(GameModuleStub::StubGameInstanceSubsystem);
    (void)sizeof(GameModuleStub::StubTickableWorldSubsystem);
    (void)sizeof(GameModuleStub::SampleTableDataAsset);
    (void)GameModuleStub::SampleTableDataAsset::StaticClass();
    return MinEngine_GameModuleEntry();
}
