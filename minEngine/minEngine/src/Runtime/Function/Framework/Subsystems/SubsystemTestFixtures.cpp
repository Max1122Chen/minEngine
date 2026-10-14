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
}
