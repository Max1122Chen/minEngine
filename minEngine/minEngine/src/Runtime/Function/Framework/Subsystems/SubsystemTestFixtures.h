#pragma once

#include "Runtime/Function/Framework/Subsystems/Subsystem.h"
#include "Runtime/Function/Framework/Subsystems/SubsystemCollection.h"

namespace minEngine
{
    /**
     * Test-only EngineSubsystem types. Gated by s_AllowCreate so production Engine
     * InitializeEngineSubsystems does not instantiate them.
     */
    ME_CLASS()
    class TestEngineSubsystemAlpha : public EngineSubsystem
    {
        ME_GENERATED_BODY()

    public:
        static bool s_AllowCreate;
        static int s_InitCount;
        static int s_DeinitCount;

        bool ShouldCreateSubsystem(MEObject* outer) const override;
        void Initialize(SubsystemCollectionBase& collection) override;
        void Deinitialize() override;
    };

    ME_CLASS()
    class TestEngineSubsystemBeta : public EngineSubsystem
    {
        ME_GENERATED_BODY()

    public:
        static bool s_AllowCreate;
        static int s_InitCount;
        static bool s_SawAlphaDuringInit;

        bool ShouldCreateSubsystem(MEObject* outer) const override;
        void Initialize(SubsystemCollectionBase& collection) override;
        void Deinitialize() override;
    };

    ME_CLASS()
    class TestGameInstanceSubsystem : public GameInstanceSubsystem
    {
        ME_GENERATED_BODY()

    public:
        static bool s_AllowCreate;
        static int s_InitCount;
        static int s_DeinitCount;

        bool ShouldCreateSubsystem(MEObject* outer) const override;
        void Initialize(SubsystemCollectionBase& collection) override;
        void Deinitialize() override;
    };
}

#include "SubsystemTestFixtures.gen.h"
