#pragma once

#include "GameModuleStubAPI.h"
#include "Runtime/Function/Framework/Subsystems/Subsystem.h"

namespace GameModuleStub
{
    /**
     * Sample GameInstanceSubsystem living in the game SHARED module.
     * Discovered via reflection when the module is loaded into the process.
     */
    ME_CLASS()
    class StubGameInstanceSubsystem : public minEngine::GameInstanceSubsystem
    {
        ME_GENERATED_BODY()

    public:
        static int s_InitCount;
        static int s_DeinitCount;

        void Initialize(minEngine::SubsystemCollectionBase& collection) override;
        void Deinitialize() override;
    };
}

#include "StubGameInstanceSubsystem.gen.h"
