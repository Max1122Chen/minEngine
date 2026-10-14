#pragma once

#include "Runtime/Function/Framework/Subsystems/Subsystem.h"

namespace GameModuleStub
{
    /**
     * Sample TickableWorldSubsystem in the game SHARED module.
     * Shows World discovery + Engine LogicalTick phase consumption.
     */
    ME_CLASS()
    class StubTickableWorldSubsystem : public minEngine::TickableWorldSubsystem
    {
        ME_GENERATED_BODY()

    public:
        static int s_InitCount;
        static int s_TickCount;

        void Initialize(minEngine::SubsystemCollectionBase& collection) override;
        void Tick(float deltaTime) override;
    };
}

#include "StubTickableWorldSubsystem.gen.h"
