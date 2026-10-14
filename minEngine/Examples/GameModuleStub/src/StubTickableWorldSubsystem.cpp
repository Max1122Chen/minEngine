#include "StubTickableWorldSubsystem.h"

namespace GameModuleStub
{
    int StubTickableWorldSubsystem::s_InitCount = 0;
    int StubTickableWorldSubsystem::s_TickCount = 0;

    void StubTickableWorldSubsystem::Initialize(minEngine::SubsystemCollectionBase& collection)
    {
        (void)collection;
        ++s_InitCount;
    }

    void StubTickableWorldSubsystem::Tick(float deltaTime)
    {
        (void)deltaTime;
        ++s_TickCount;
    }
}
