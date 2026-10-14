#include "BeginPlayTest.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"

namespace minEngine
{
    class BeginPlayTestScope
    {
    public:
        BeginPlayTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();

            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~BeginPlayTestScope()
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

    namespace
    {
        bool RunDispatchAndEndPlayTest()
        {
            BeginPlayTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("begin-play");
            if (!scene)
            {
                ME_LOG(LogTest, Error, "BeginPlayTest: failed to create scene.");
                return false;
            }

            const std::shared_ptr<GameObject> go = scene->CreateGameObject();
            const std::shared_ptr<SceneComponent> comp = go->AddComponent<SceneComponent>();
            if (!comp || !comp->IsActive())
            {
                ME_LOG(LogTest, Error, "BeginPlayTest: component not active after add.");
                return false;
            }

            if (go->HasBegunPlay() || comp->HasBegunPlay() || scene->HasBegunPlay())
            {
                ME_LOG(LogTest, Error, "BeginPlayTest: BeginPlay flags set before Dispatch.");
                return false;
            }

            scene->DispatchBeginPlay();
            if (!scene->HasBegunPlay() || !go->HasBegunPlay() || !comp->HasBegunPlay())
            {
                ME_LOG(LogTest, Error, "BeginPlayTest: DispatchBeginPlay did not set HasBegunPlay.");
                return false;
            }

            if (!SceneManager::Get().HasBegunPlayWorld())
            {
                ME_LOG(LogTest, Error, "BeginPlayTest: HasBegunPlayWorld false after Dispatch.");
                return false;
            }

            scene->DispatchBeginPlay();
            scene->DispatchEndPlay(EEndPlayReason::Stopped);
            if (scene->HasBegunPlay() || go->HasBegunPlay() || comp->HasBegunPlay())
            {
                ME_LOG(LogTest, Error, "BeginPlayTest: DispatchEndPlay did not clear HasBegunPlay.");
                return false;
            }

            return true;
        }

        bool RunInactiveSkipsBeginPlayTest()
        {
            BeginPlayTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("begin-play-inactive");
            const std::shared_ptr<GameObject> go = scene->CreateGameObject();
            const std::shared_ptr<SceneComponent> activeComp = go->AddComponent<SceneComponent>();
            const std::shared_ptr<SceneComponent> inactiveComp = go->AddComponent<SceneComponent>();
            inactiveComp->SetActive(false);

            scene->DispatchBeginPlay();
            if (!activeComp->HasBegunPlay() || inactiveComp->HasBegunPlay())
            {
                ME_LOG(LogTest, Error, "BeginPlayTest: inactive component BeginPlay eligibility wrong.");
                return false;
            }

            inactiveComp->SetActive(true);
            if (!inactiveComp->HasBegunPlay())
            {
                ME_LOG(LogTest, Error, "BeginPlayTest: late Activate did not BeginPlay while simulating.");
                return false;
            }

            return true;
        }

        bool RunDynamicSpawnBeginPlayTest()
        {
            BeginPlayTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("begin-play-spawn");
            scene->DispatchBeginPlay();

            const std::shared_ptr<GameObject> go = scene->CreateGameObject();
            if (!go->HasBegunPlay())
            {
                ME_LOG(LogTest, Error, "BeginPlayTest: CreateGameObject after Dispatch did not BeginPlay GO.");
                return false;
            }

            const std::shared_ptr<SceneComponent> comp = go->AddComponent<SceneComponent>();
            if (!comp->HasBegunPlay())
            {
                ME_LOG(LogTest, Error, "BeginPlayTest: AddComponent after sim BeginPlay did not BeginPlay Comp.");
                return false;
            }

            return true;
        }
    }

    bool RunBeginPlayTests()
    {
        return RunDispatchAndEndPlayTest()
            && RunInactiveSkipsBeginPlayTest()
            && RunDynamicSpawnBeginPlayTest();
    }
}

#include "doctest.h"
#include "EngineTestFixture.h"

TEST_CASE("begin-play: dispatch endplay inactive dynamic [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunBeginPlayTests());
}
