#include "EngineTestFixture.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Engine.h"
#include "Runtime/Function/Framework/GameInstance/GameInstance.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

#include "doctest.h"

#if defined(MINENGINE_LINK_GAME_MODULE_STUB) && MINENGINE_LINK_GAME_MODULE_STUB
#    include "StubGameInstanceSubsystem.h"
#    include "StubTickableWorldSubsystem.h"
#endif

namespace minEngine
{
#if defined(MINENGINE_LINK_GAME_MODULE_STUB) && MINENGINE_LINK_GAME_MODULE_STUB
    namespace
    {
        class GameModuleStubSubsystemScope
        {
        public:
            GameModuleStubSubsystemScope()
            {
                Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
                ObjectManager::Initialize();
                Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
                m_SceneManager.Initialize();

                GameModuleStub::StubGameInstanceSubsystem::s_InitCount = 0;
                GameModuleStub::StubGameInstanceSubsystem::s_DeinitCount = 0;
                GameModuleStub::StubTickableWorldSubsystem::s_InitCount = 0;
                GameModuleStub::StubTickableWorldSubsystem::s_TickCount = 0;
            }

            ~GameModuleStubSubsystemScope()
            {
                m_SceneManager.Shutdown();
                Testing::TestAccess<SceneManager>::SetInstance(nullptr);
                ObjectManager::Shutdown();
                Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
            }

        private:
            ObjectManager m_ObjectManager;
            SceneManager m_SceneManager;
        };
    }

    TEST_CASE("game-module-stub-subsystem [game-module-stub]" *
              doctest::description("CORE-F34 S06: external GI/World subsystem discovery"))
    {
        EngineReflectionFixture reflectionFixture;
        REQUIRE(reflectionFixture.IsReflectionReady());

        GameModuleStubSubsystemScope scope;

        const Reflection::MEClass* giClass =
            Reflection::ReflectionSystem::Get().FindClass<GameModuleStub::StubGameInstanceSubsystem>();
        const Reflection::MEClass* worldClass =
            Reflection::ReflectionSystem::Get().FindClass<GameModuleStub::StubTickableWorldSubsystem>();
        REQUIRE(giClass != nullptr);
        REQUIRE(worldClass != nullptr);

        Engine engine;
        engine.InitializeEngineSubsystems();

        std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("stub-subsystem-scene");
        REQUIRE(scene != nullptr);

        GameModuleStub::StubTickableWorldSubsystem* worldSubsystem =
            scene->GetSubsystem<GameModuleStub::StubTickableWorldSubsystem>();
        REQUIRE(worldSubsystem != nullptr);
        CHECK(GameModuleStub::StubTickableWorldSubsystem::s_InitCount >= 1);

        std::shared_ptr<GameInstance> gi = engine.CreateGameInstance(scene.get());
        REQUIRE(gi != nullptr);

        GameModuleStub::StubGameInstanceSubsystem* giSubsystem =
            gi->GetSubsystem<GameModuleStub::StubGameInstanceSubsystem>();
        REQUIRE(giSubsystem != nullptr);
        CHECK(GameModuleStub::StubGameInstanceSubsystem::s_InitCount >= 1);

        engine.TickTickableSubsystems(0.016f);
        CHECK(GameModuleStub::StubTickableWorldSubsystem::s_TickCount >= 1);

        engine.DestroyGameInstance();
        CHECK(GameModuleStub::StubGameInstanceSubsystem::s_DeinitCount >= 1);

        engine.DeinitializeEngineSubsystems();
    }
#else
    TEST_CASE("game-module-stub-subsystem-skipped [game-module-stub]" *
              doctest::description("Skipped unless GameModuleStub is linked into minEngineTests"))
    {
        CHECK(true);
    }
#endif
}
