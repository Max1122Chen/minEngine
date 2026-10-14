#pragma once

#include "Runtime/Function/Framework/GameInstance/GameInstance.h"
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

    ME_CLASS()
    class TestWorldSubsystem : public WorldSubsystem
    {
        ME_GENERATED_BODY()

    public:
        static bool s_AllowCreate;
        static int s_InitCount;
        static int s_DeinitCount;
        static int s_PostInitCount;
        static int s_BeginPlayCount;
        static int s_EndPlayCount;

        bool ShouldCreateSubsystem(MEObject* outer) const override;
        void Initialize(SubsystemCollectionBase& collection) override;
        void Deinitialize() override;
        void PostInitialize() override;
        void OnWorldBeginPlay(Scene& scene) override;
        void OnWorldEndPlay(Scene& scene, EEndPlayReason reason) override;
    };

    ME_CLASS()
    class TestTickableWorldSubsystem : public TickableWorldSubsystem
    {
        ME_GENERATED_BODY()

    public:
        static bool s_AllowCreate;
        static int s_TickCount;
        static float s_LastDeltaTime;

        bool ShouldCreateSubsystem(MEObject* outer) const override;
        void Tick(float deltaTime) override;
    };

    ME_CLASS()
    class TestTickableGameInstanceSubsystem : public TickableGameInstanceSubsystem
    {
        ME_GENERATED_BODY()

    public:
        static bool s_AllowCreate;
        static int s_TickCount;
        static float s_LastDeltaTime;

        bool ShouldCreateSubsystem(MEObject* outer) const override;
        void Tick(float deltaTime) override;
    };

    /** CORE-F35: derived hub for Resolve/Create tests. */
    ME_CLASS()
    class TestDerivedGameInstance : public GameInstance
    {
        ME_GENERATED_BODY()

    public:
        static int s_OnInitCount;
        static bool s_SawGiSubsystemOnInit;

        void OnInit() override;
    };
}

#include "SubsystemTestFixtures.gen.h"
