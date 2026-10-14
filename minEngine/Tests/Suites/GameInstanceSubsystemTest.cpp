#include "GameInstanceSubsystemTest.h"
#include "Access/ObjectManagerTestAccess.h"

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
    class GameInstanceSubsystemTestScope
    {
    public:
        GameInstanceSubsystemTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();

            TestEngineSubsystemAlpha::s_AllowCreate = false;
            TestEngineSubsystemAlpha::s_InitCount = 0;
            TestEngineSubsystemAlpha::s_DeinitCount = 0;
            TestEngineSubsystemBeta::s_AllowCreate = false;
            TestEngineSubsystemBeta::s_InitCount = 0;
            TestEngineSubsystemBeta::s_SawAlphaDuringInit = false;
            TestGameInstanceSubsystem::s_AllowCreate = false;
            TestGameInstanceSubsystem::s_InitCount = 0;
            TestGameInstanceSubsystem::s_DeinitCount = 0;
            TestWorldSubsystem::s_AllowCreate = false;
            TestWorldSubsystem::s_InitCount = 0;
            TestWorldSubsystem::s_DeinitCount = 0;
            TestWorldSubsystem::s_PostInitCount = 0;
            TestTickableWorldSubsystem::s_AllowCreate = false;
            TestTickableGameInstanceSubsystem::s_AllowCreate = false;
        }

        ~GameInstanceSubsystemTestScope()
        {
            TestEngineSubsystemAlpha::s_AllowCreate = false;
            TestEngineSubsystemBeta::s_AllowCreate = false;
            TestGameInstanceSubsystem::s_AllowCreate = false;
            TestWorldSubsystem::s_AllowCreate = false;
            TestTickableWorldSubsystem::s_AllowCreate = false;
            TestTickableGameInstanceSubsystem::s_AllowCreate = false;

            ObjectManager::Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

    private:
        ObjectManager m_ObjectManager;
    };

    namespace
    {
        bool RunEngineSubsystemInitDeinitTest()
        {
            GameInstanceSubsystemTestScope scope;

            TestEngineSubsystemAlpha::s_AllowCreate = true;
            TestEngineSubsystemBeta::s_AllowCreate = true;

            Engine engine;
            engine.InitializeEngineSubsystems();

            TestEngineSubsystemAlpha* alpha = engine.GetEngineSubsystem<TestEngineSubsystemAlpha>();
            TestEngineSubsystemBeta* beta = engine.GetEngineSubsystem<TestEngineSubsystemBeta>();
            if (alpha == nullptr || beta == nullptr)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: engine subsystems not created.");
                return false;
            }
            if (TestEngineSubsystemAlpha::s_InitCount < 1 || TestEngineSubsystemBeta::s_InitCount < 1)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: Initialize not called.");
                return false;
            }
            if (!TestEngineSubsystemBeta::s_SawAlphaDuringInit)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: InitializeDependency failed.");
                return false;
            }

            engine.DeinitializeEngineSubsystems();
            if (TestEngineSubsystemAlpha::s_DeinitCount < 1)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: Deinitialize not called.");
                return false;
            }
            if (engine.GetEngineSubsystem<TestEngineSubsystemAlpha>() != nullptr)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: subsystem still reachable after Deinit.");
                return false;
            }

            return true;
        }

        bool RunGameInstanceCreateShutdownTest()
        {
            GameInstanceSubsystemTestScope scope;

            TestGameInstanceSubsystem::s_AllowCreate = true;

            Engine engine;
            engine.InitializeEngineSubsystems();

            std::shared_ptr<GameInstance> gi = engine.CreateGameInstance();
            if (!gi)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: CreateGameInstance failed.");
                return false;
            }
            if (engine.GetGameInstance() != gi.get())
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: GetGameInstance mismatch.");
                return false;
            }

            TestGameInstanceSubsystem* giSubsystem = gi->GetSubsystem<TestGameInstanceSubsystem>();
            if (giSubsystem == nullptr || TestGameInstanceSubsystem::s_InitCount < 1)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: GI subsystem not created.");
                return false;
            }

            engine.DestroyGameInstance();
            if (TestGameInstanceSubsystem::s_DeinitCount < 1)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: GI subsystem Deinitialize missing.");
                return false;
            }
            if (engine.GetGameInstance() != nullptr)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: GameInstance still set after Destroy.");
                return false;
            }

            engine.DeinitializeEngineSubsystems();
            return true;
        }

        bool RunGatedFixtureSkippedInDefaultDiscoveryTest()
        {
            GameInstanceSubsystemTestScope scope;

            Engine engine;
            engine.InitializeEngineSubsystems();

            if (engine.GetEngineSubsystem<TestEngineSubsystemAlpha>() != nullptr
                || engine.GetEngineSubsystem<TestEngineSubsystemBeta>() != nullptr)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: gated fixtures created unexpectedly.");
                return false;
            }
            if (TestEngineSubsystemAlpha::s_InitCount != 0)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: gated Alpha InitCount != 0.");
                return false;
            }

            engine.DeinitializeEngineSubsystems();
            return true;
        }

        bool RunPieGameInstanceIsolationAndOrderTest()
        {
            GameInstanceSubsystemTestScope scope;

            TestGameInstanceSubsystem::s_AllowCreate = true;
            TestWorldSubsystem::s_AllowCreate = true;

            Engine engine;
            engine.InitializeEngineSubsystems();

            std::shared_ptr<Scene> pieScene0 = NewObject<Scene>("PIE_0");
            pieScene0->SetSceneType(ESceneType::PIE);
            pieScene0->SetPIEInstanceId(0);
            SceneManager::FinalizeSceneStructure(pieScene0.get());

            std::shared_ptr<GameInstance> gi0 =
                engine.CreateGameInstanceForPIE(0, pieScene0.get());
            if (!gi0 || engine.GetGameInstanceForPIE(0) != gi0.get())
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: PIE0 Create/Get failed.");
                return false;
            }
            if (gi0->GetPrimaryScene() != pieScene0.get() || gi0->GetPIEInstanceId() != 0)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: PIE0 binding mismatch.");
                return false;
            }
            if (TestGameInstanceSubsystem::s_InitCount < 1)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: GI Init before World missing.");
                return false;
            }
            if (TestWorldSubsystem::s_InitCount != 0)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: World Init ran before Play wiring.");
                return false;
            }

            pieScene0->InitializeWorldSubsystems();
            pieScene0->PostInitializeWorldSubsystems();
            if (TestWorldSubsystem::s_InitCount < 1 || TestWorldSubsystem::s_PostInitCount < 1)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: World Init/PostInit after GI failed.");
                return false;
            }

            std::shared_ptr<Scene> pieScene1 = NewObject<Scene>("PIE_1");
            pieScene1->SetSceneType(ESceneType::PIE);
            pieScene1->SetPIEInstanceId(1);
            SceneManager::FinalizeSceneStructure(pieScene1.get());

            std::shared_ptr<GameInstance> gi1 =
                engine.CreateGameInstanceForPIE(1, pieScene1.get());
            if (!gi1 || gi1 == gi0 || engine.GetGameInstanceForPIE(1) != gi1.get())
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: PIE1 isolation failed.");
                return false;
            }

            pieScene0->DeinitializeWorldSubsystems();
            engine.DestroyGameInstanceForPIE(0);
            if (engine.GetGameInstanceForPIE(0) != nullptr || engine.GetGameInstanceForPIE(1) == nullptr)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: Destroy PIE0 leaked or wiped PIE1.");
                return false;
            }

            pieScene1->DeinitializeWorldSubsystems();
            engine.DestroyAllPIEGameInstances();
            if (engine.GetGameInstanceForPIE(1) != nullptr)
            {
                ME_LOG(LogTest, Error, "GameInstanceSubsystemTest: DestroyAllPIE left instances.");
                return false;
            }

            engine.DeinitializeEngineSubsystems();
            return true;
        }
    }

    bool RunGameInstanceSubsystemTests()
    {
        return RunEngineSubsystemInitDeinitTest()
            && RunGameInstanceCreateShutdownTest()
            && RunGatedFixtureSkippedInDefaultDiscoveryTest()
            && RunPieGameInstanceIsolationAndOrderTest();
    }
}

TEST_CASE("game-instance-subsystem: collection init dependency gi [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunGameInstanceSubsystemTests());
}
