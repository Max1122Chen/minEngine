#include "StubGameInstanceSubsystem.h"

namespace GameModuleStub
{
    int StubGameInstanceSubsystem::s_InitCount = 0;
    int StubGameInstanceSubsystem::s_DeinitCount = 0;

    void StubGameInstanceSubsystem::Initialize(minEngine::SubsystemCollectionBase& collection)
    {
        (void)collection;
        ++s_InitCount;
    }

    void StubGameInstanceSubsystem::Deinitialize()
    {
        ++s_DeinitCount;
    }
}
