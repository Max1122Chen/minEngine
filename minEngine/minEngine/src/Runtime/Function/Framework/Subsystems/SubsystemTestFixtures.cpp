#include "SubsystemTestFixtures.h"

namespace minEngine
{
    bool TestEngineSubsystemAlpha::s_AllowCreate = false;
    int TestEngineSubsystemAlpha::s_InitCount = 0;
    int TestEngineSubsystemAlpha::s_DeinitCount = 0;

    bool TestEngineSubsystemAlpha::ShouldCreateSubsystem(MEObject* outer) const
    {
        (void)outer;
        return s_AllowCreate;
    }

    void TestEngineSubsystemAlpha::Initialize(SubsystemCollectionBase& collection)
    {
        (void)collection;
        ++s_InitCount;
    }

    void TestEngineSubsystemAlpha::Deinitialize()
    {
        ++s_DeinitCount;
    }

    bool TestEngineSubsystemBeta::s_AllowCreate = false;
    int TestEngineSubsystemBeta::s_InitCount = 0;
    bool TestEngineSubsystemBeta::s_SawAlphaDuringInit = false;

    bool TestEngineSubsystemBeta::ShouldCreateSubsystem(MEObject* outer) const
    {
        (void)outer;
        return s_AllowCreate;
    }

    void TestEngineSubsystemBeta::Initialize(SubsystemCollectionBase& collection)
    {
        TestEngineSubsystemAlpha* alpha = collection.InitializeDependency<TestEngineSubsystemAlpha>();
        s_SawAlphaDuringInit = (alpha != nullptr);
        ++s_InitCount;
    }

    void TestEngineSubsystemBeta::Deinitialize() {}

    bool TestGameInstanceSubsystem::s_AllowCreate = false;
    int TestGameInstanceSubsystem::s_InitCount = 0;
    int TestGameInstanceSubsystem::s_DeinitCount = 0;

    bool TestGameInstanceSubsystem::ShouldCreateSubsystem(MEObject* outer) const
    {
        (void)outer;
        return s_AllowCreate;
    }

    void TestGameInstanceSubsystem::Initialize(SubsystemCollectionBase& collection)
    {
        (void)collection;
        ++s_InitCount;
    }

    void TestGameInstanceSubsystem::Deinitialize()
    {
        ++s_DeinitCount;
    }

    bool TestWorldSubsystem::s_AllowCreate = false;
    int TestWorldSubsystem::s_InitCount = 0;
    int TestWorldSubsystem::s_DeinitCount = 0;
    int TestWorldSubsystem::s_PostInitCount = 0;
    int TestWorldSubsystem::s_BeginPlayCount = 0;
    int TestWorldSubsystem::s_EndPlayCount = 0;

    bool TestWorldSubsystem::ShouldCreateSubsystem(MEObject* outer) const
    {
        if (!s_AllowCreate)
        {
            return false;
        }
        return WorldSubsystem::ShouldCreateSubsystem(outer);
    }

    void TestWorldSubsystem::Initialize(SubsystemCollectionBase& collection)
    {
        (void)collection;
        ++s_InitCount;
    }

    void TestWorldSubsystem::Deinitialize()
    {
        ++s_DeinitCount;
    }

    void TestWorldSubsystem::PostInitialize()
    {
        ++s_PostInitCount;
    }

    void TestWorldSubsystem::OnWorldBeginPlay(Scene& scene)
    {
        (void)scene;
        ++s_BeginPlayCount;
    }

    void TestWorldSubsystem::OnWorldEndPlay(Scene& scene, EEndPlayReason reason)
    {
        (void)scene;
        (void)reason;
        ++s_EndPlayCount;
    }

    bool TestTickableWorldSubsystem::s_AllowCreate = false;
    int TestTickableWorldSubsystem::s_TickCount = 0;
    float TestTickableWorldSubsystem::s_LastDeltaTime = 0.0f;

    bool TestTickableWorldSubsystem::ShouldCreateSubsystem(MEObject* outer) const
    {
        if (!s_AllowCreate)
        {
            return false;
        }
        return TickableWorldSubsystem::ShouldCreateSubsystem(outer);
    }

    void TestTickableWorldSubsystem::Tick(float deltaTime)
    {
        ++s_TickCount;
        s_LastDeltaTime = deltaTime;
    }

    bool TestTickableGameInstanceSubsystem::s_AllowCreate = false;
    int TestTickableGameInstanceSubsystem::s_TickCount = 0;
    float TestTickableGameInstanceSubsystem::s_LastDeltaTime = 0.0f;

    bool TestTickableGameInstanceSubsystem::ShouldCreateSubsystem(MEObject* outer) const
    {
        (void)outer;
        return s_AllowCreate;
    }

    void TestTickableGameInstanceSubsystem::Tick(float deltaTime)
    {
        ++s_TickCount;
        s_LastDeltaTime = deltaTime;
    }

    int TestDerivedGameInstance::s_OnInitCount = 0;
    bool TestDerivedGameInstance::s_SawGiSubsystemOnInit = false;

    void TestDerivedGameInstance::OnInit()
    {
        ++s_OnInitCount;
        s_SawGiSubsystemOnInit = GetSubsystem<TestGameInstanceSubsystem>() != nullptr;
    }
}
