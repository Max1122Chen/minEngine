#include "TickableSubsystemTest.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Engine.h"
#include "Runtime/Function/Framework/GameInstance/GameInstance.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"
#include "Runtime/Function/Framework/Subsystems/SubsystemTestFixtures.h"

#include "doctest.h"
#include "EngineTestFixture.h"

namespace minEngine
{
    class TickableSubsystemTestScope
    {
    public:
        TickableSubsystemTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();

            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            TestTickableWorldSubsystem::s_AllowCreate = false;
            TestTickableWorldSubsystem::s_TickCount = 0;
            TestTickableWorldSubsystem::s_LastDeltaTime = 0.0f;
            TestTickableGameInstanceSubsystem::s_AllowCreate = false;
            TestTickableGameInstanceSubsystem::s_TickCount = 0;
            TestTickableGameInstanceSubsystem::s_LastDeltaTime = 0.0f;
        }

        ~TickableSubsystemTestScope()
        {
            TestTickableWorldSubsystem::s_AllowCreate = false;
            TestTickableGameInstanceSubsystem::s_AllowCreate = false;

            m_SceneManager.Shutdown();
            Testing::TestAccess<SceneManager>::SetInstance(nullptr);

            ObjectManager::Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
    };

    namespace
    {
        bool RunTickPhaseTest()
        {
            TickableSubsystemTestScope scope;

            TestTickableWorldSubsystem::s_AllowCreate = true;
            TestTickableGameInstanceSubsystem::s_AllowCreate = true;

            Engine engine;
            engine.InitializeEngineSubsystems();

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("tickable-subsystems");
            if (!scene || scene->GetSubsystem<TestTickableWorldSubsystem>() == nullptr)
            {
                ME_LOG(LogTest, Error, "TickableSubsystemTest: world tickable not created.");
                return false;
            }

            // CreateNewScene sets Editor context with Gameplay tick policy — TickWorldSubsystems sees it.
            std::shared_ptr<GameInstance> gi = engine.CreateGameInstance(scene.get());
            if (!gi || gi->GetSubsystem<TestTickableGameInstanceSubsystem>() == nullptr)
            {
                ME_LOG(LogTest, Error, "TickableSubsystemTest: GI tickable not created.");
                return false;
            }

            constexpr float kDelta = 0.016f;
            engine.TickTickableSubsystems(kDelta);

            if (TestTickableWorldSubsystem::s_TickCount != 1
                || TestTickableGameInstanceSubsystem::s_TickCount != 1)
            {
                ME_LOG(LogTest, Error, "TickableSubsystemTest: expected one tick each (world={}, gi={}).",
                    TestTickableWorldSubsystem::s_TickCount,
                    TestTickableGameInstanceSubsystem::s_TickCount);
                return false;
            }
            if (TestTickableWorldSubsystem::s_LastDeltaTime != kDelta
                || TestTickableGameInstanceSubsystem::s_LastDeltaTime != kDelta)
            {
                ME_LOG(LogTest, Error, "TickableSubsystemTest: deltaTime not forwarded.");
                return false;
            }

            engine.DestroyGameInstance();
            engine.DeinitializeEngineSubsystems();
            return true;
        }
    }

    bool RunTickableSubsystemTests()
    {
        return RunTickPhaseTest();
    }
}

TEST_CASE("tickable-subsystem: logical tick phase [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunTickableSubsystemTests());
}
