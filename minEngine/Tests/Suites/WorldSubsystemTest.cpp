#include "WorldSubsystemTest.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/Subsystems/SubsystemTestFixtures.h"

#include "doctest.h"
#include "EngineTestFixture.h"

namespace minEngine
{
    class WorldSubsystemTestScope
    {
    public:
        WorldSubsystemTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();

            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            TestWorldSubsystem::s_AllowCreate = false;
            TestWorldSubsystem::s_InitCount = 0;
            TestWorldSubsystem::s_DeinitCount = 0;
            TestWorldSubsystem::s_PostInitCount = 0;
            TestWorldSubsystem::s_BeginPlayCount = 0;
            TestWorldSubsystem::s_EndPlayCount = 0;
            TestTickableWorldSubsystem::s_AllowCreate = false;
            TestTickableGameInstanceSubsystem::s_AllowCreate = false;
        }

        ~WorldSubsystemTestScope()
        {
            TestWorldSubsystem::s_AllowCreate = false;
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
        bool RunInitPostInitAndPlayHooksTest()
        {
            WorldSubsystemTestScope scope;
            TestWorldSubsystem::s_AllowCreate = true;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("world-subsystem");
            if (!scene)
            {
                ME_LOG(LogTest, Error, "WorldSubsystemTest: CreateNewScene failed.");
                return false;
            }

            TestWorldSubsystem* worldSubsystem = scene->GetSubsystem<TestWorldSubsystem>();
            if (worldSubsystem == nullptr)
            {
                ME_LOG(LogTest, Error, "WorldSubsystemTest: subsystem not created on Finalize.");
                return false;
            }
            if (TestWorldSubsystem::s_InitCount < 1 || TestWorldSubsystem::s_PostInitCount < 1)
            {
                ME_LOG(LogTest, Error, "WorldSubsystemTest: Init/PostInit counts wrong.");
                return false;
            }
            if (worldSubsystem->GetScene() != scene.get())
            {
                ME_LOG(LogTest, Error, "WorldSubsystemTest: GetScene mismatch.");
                return false;
            }

            scene->DispatchBeginPlay();
            if (TestWorldSubsystem::s_BeginPlayCount < 1)
            {
                ME_LOG(LogTest, Error, "WorldSubsystemTest: OnWorldBeginPlay not called.");
                return false;
            }

            scene->DispatchEndPlay(EEndPlayReason::Stopped);
            if (TestWorldSubsystem::s_EndPlayCount < 1)
            {
                ME_LOG(LogTest, Error, "WorldSubsystemTest: OnWorldEndPlay not called.");
                return false;
            }

            SceneManager::Get().UnloadActiveScene();
            if (TestWorldSubsystem::s_DeinitCount < 1)
            {
                ME_LOG(LogTest, Error, "WorldSubsystemTest: Deinitialize not called on unload.");
                return false;
            }

            return true;
        }

        bool RunGatedFixtureSkippedTest()
        {
            WorldSubsystemTestScope scope;
            // s_AllowCreate remains false

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("world-subsystem-gated");
            if (!scene)
            {
                ME_LOG(LogTest, Error, "WorldSubsystemTest: gated CreateNewScene failed.");
                return false;
            }

            if (scene->GetSubsystem<TestWorldSubsystem>() != nullptr || TestWorldSubsystem::s_InitCount != 0)
            {
                ME_LOG(LogTest, Error, "WorldSubsystemTest: gated fixture was created.");
                return false;
            }

            return true;
        }
    }

    bool RunWorldSubsystemTests()
    {
        return RunInitPostInitAndPlayHooksTest() && RunGatedFixtureSkippedTest();
    }
}

TEST_CASE("world-subsystem: init postinit begin endplay [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunWorldSubsystemTests());
}
