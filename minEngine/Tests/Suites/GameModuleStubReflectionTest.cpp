#include "EngineTestFixture.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

#include "doctest.h"

#if defined(MINENGINE_LINK_GAME_MODULE_STUB) && MINENGINE_LINK_GAME_MODULE_STUB
#    include "StubGamePrimitiveComponent.h"
#endif

namespace minEngine
{
#if defined(MINENGINE_LINK_GAME_MODULE_STUB) && MINENGINE_LINK_GAME_MODULE_STUB
    namespace
    {
        class GameModuleStubReflectionScope
        {
        public:
            GameModuleStubReflectionScope()
            {
                Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
                m_ObjectManager.Initialize();
                Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
                m_SceneManager.Initialize();
            }

            ~GameModuleStubReflectionScope()
            {
                m_SceneManager.Shutdown();
                Testing::TestAccess<SceneManager>::SetInstance(nullptr);
                m_ObjectManager.Shutdown();
                Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
            }

        private:
            ObjectManager m_ObjectManager;
            SceneManager m_SceneManager;
        };
    }

    TEST_CASE("game-module-stub-reflection [game-module-stub]" *
              doctest::description("CORE-F27: external ME_CLASS AddComponent path"))
    {
        EngineReflectionFixture reflectionFixture;
        (void)reflectionFixture;

        GameModuleStubReflectionScope scope;

        const Reflection::MEClass* stubClass =
            Reflection::ReflectionSystem::Get().FindClass<GameModuleStub::StubGamePrimitiveComponent>();
        REQUIRE(stubClass != nullptr);

        std::shared_ptr<Scene> scene = NewObject<Scene>("StubReflectionScene");
        REQUIRE(scene != nullptr);
        std::shared_ptr<GameObject> go = scene->CreateGameObject();
        REQUIRE(go != nullptr);
        go->Rename("StubGo");

        std::shared_ptr<GameModuleStub::StubGamePrimitiveComponent> prim =
            go->AddComponent<GameModuleStub::StubGamePrimitiveComponent>();
        REQUIRE(prim != nullptr);
        CHECK(prim->GetClass() != nullptr);
        CHECK(prim->GetOwner() == go.get());
        CHECK(go->GetRootComponent() == prim.get());
    }
#else
    TEST_CASE("game-module-stub-reflection-skipped [game-module-stub]" *
              doctest::description("Skipped unless GameModuleStub is linked into minEngineTests"))
    {
        CHECK(true);
    }
#endif
}
