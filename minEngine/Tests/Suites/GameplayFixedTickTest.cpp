#include "GameplayFixedTickTest.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Engine.h"
#include "Runtime/Function/Framework/GameInstance/GameInstance.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/Subsystems/SubsystemTestFixtures.h"

#include "doctest.h"
#include "EngineTestFixture.h"

namespace minEngine
{
    class GameplayFixedTickTestScope
    {
    public:
        GameplayFixedTickTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();

            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            TestTickableWorldSubsystem::s_AllowCreate = false;
            TestTickableWorldSubsystem::s_TickCount = 0;
            TestTickableWorldSubsystem::s_FixedTickCount = 0;
            TestTickableGameInstanceSubsystem::s_AllowCreate = false;
            TestTickableGameInstanceSubsystem::s_TickCount = 0;
            TestTickableGameInstanceSubsystem::s_FixedTickCount = 0;
        }

        ~GameplayFixedTickTestScope()
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
        bool RunFixedClockAndWorldDispatch()
        {
            GameplayFixedTickTestScope scope;

            TestTickableWorldSubsystem::s_AllowCreate = true;
            TestTickableGameInstanceSubsystem::s_AllowCreate = true;

            Engine engine;
            engine.InitializeEngineSubsystems();
            engine.SetGameInstanceClassOverride(TestFixedTickGameInstance::StaticClass());

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("gameplay-fixed-tick");
            if (!scene || scene->GetSubsystem<TestTickableWorldSubsystem>() == nullptr)
            {
                ME_LOG(LogTest, Error, "GameplayFixedTickTest: world tickable missing.");
                return false;
            }

            std::shared_ptr<GameInstance> gi = engine.CreateGameInstance(scene.get());
            if (!gi || gi->GetSubsystem<TestTickableGameInstanceSubsystem>() == nullptr)
            {
                ME_LOG(LogTest, Error, "GameplayFixedTickTest: GI tickable missing.");
                return false;
            }
            if (gi->GetClass() != TestFixedTickGameInstance::StaticClass())
            {
                ME_LOG(LogTest, Error, "GameplayFixedTickTest: expected TestFixedTickGameInstance.");
                return false;
            }

            engine.SetGameplayFixedTickInterval(0.05f);
            engine.SetGameplayFixedMaxStepsPerFrame(8);

            // One frame of 0.12s → 2 fixed steps at 0.05, remainder 0.02.
            engine.TickGameplayFixedPhase(0.12f);

            if (engine.GetGameplayFixedTickIndex() != 2
                || TestTickableWorldSubsystem::s_FixedTickCount != 2
                || TestTickableGameInstanceSubsystem::s_FixedTickCount != 2)
            {
                ME_LOG(LogTest, Error,
                    "GameplayFixedTickTest: expected 2 fixed steps (index={}, world={}, gi={}).",
                    engine.GetGameplayFixedTickIndex(),
                    TestTickableWorldSubsystem::s_FixedTickCount,
                    TestTickableGameInstanceSubsystem::s_FixedTickCount);
                return false;
            }

            if (TestTickableWorldSubsystem::s_LastFixedDeltaTime != 0.05f
                || TestTickableGameInstanceSubsystem::s_LastFixedDeltaTime != 0.05f)
            {
                ME_LOG(LogTest, Error, "GameplayFixedTickTest: fixedDt mismatch.");
                return false;
            }

            engine.SetGameplayFixedTickPaused(true);
            const uint64_t indexBeforePause = engine.GetGameplayFixedTickIndex();
            engine.TickGameplayFixedPhase(1.0f);
            if (engine.GetGameplayFixedTickIndex() != indexBeforePause
                || TestTickableWorldSubsystem::s_FixedTickCount != 2)
            {
                ME_LOG(LogTest, Error, "GameplayFixedTickTest: pause should freeze FixedTick.");
                return false;
            }

            engine.DestroyGameInstance();
            engine.DeinitializeEngineSubsystems();
            return true;
        }

        bool RunVariablePhaseStillTicks()
        {
            GameplayFixedTickTestScope scope;

            TestTickableWorldSubsystem::s_AllowCreate = true;
            TestTickableGameInstanceSubsystem::s_AllowCreate = true;

            Engine engine;
            engine.InitializeEngineSubsystems();

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("gameplay-var-phase");
            std::shared_ptr<GameInstance> gi = engine.CreateGameInstance(scene.get());
            if (!gi)
            {
                return false;
            }

            engine.TickGameplayVariablePhase(0.016f);
            if (TestTickableWorldSubsystem::s_TickCount != 1
                || TestTickableGameInstanceSubsystem::s_TickCount != 1)
            {
                ME_LOG(LogTest, Error, "GameplayFixedTickTest: variable phase tick counts wrong.");
                return false;
            }

            engine.DestroyGameInstance();
            engine.DeinitializeEngineSubsystems();
            return true;
        }
    }

    bool RunGameplayFixedTickTests()
    {
        return RunFixedClockAndWorldDispatch() && RunVariablePhaseStillTicks();
    }
}

TEST_CASE("gameplay-fixed-tick: clock and layered dispatch [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunGameplayFixedTickTests());
}
