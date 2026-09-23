#include "PlayMainCameraTest.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/CameraComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"

namespace minEngine
{
    class PlayMainCameraTestScope
    {
    public:
        PlayMainCameraTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();
            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~PlayMainCameraTestScope()
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

    namespace
    {
        bool RunSoleCameraAutoMainOnBeginPlay()
        {
            PlayMainCameraTestScope scope;
            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("cam-sole");
            const std::shared_ptr<GameObject> go = scene->CreateGameObject();
            const std::shared_ptr<CameraComponent> camera = go->AddComponent<CameraComponent>();

            if (camera->IsMainCamera())
            {
                ME_LOG(LogTest, Error, "PlayMainCamera: sole camera should not be Main before BeginPlay.");
                return false;
            }

            scene->DispatchBeginPlay();
            if (!camera->IsMainCamera())
            {
                ME_LOG(LogTest, Error, "PlayMainCamera: sole active camera should become Main on BeginPlay.");
                return false;
            }
            return true;
        }

        bool RunBoolForcesMainAmongMany()
        {
            PlayMainCameraTestScope scope;
            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("cam-bool");
            const std::shared_ptr<GameObject> goA = scene->CreateGameObject();
            const std::shared_ptr<GameObject> goB = scene->CreateGameObject();
            const std::shared_ptr<CameraComponent> camA = goA->AddComponent<CameraComponent>();
            const std::shared_ptr<CameraComponent> camB = goB->AddComponent<CameraComponent>();
            camB->SetSetAsMainCameraOnBeginPlay(true);

            scene->DispatchBeginPlay();
            if (camA->IsMainCamera() || !camB->IsMainCamera())
            {
                ME_LOG(LogTest, Error, "PlayMainCamera: bSetAsMainCameraOnBeginPlay should pick that camera as Main.");
                return false;
            }
            return true;
        }

        bool RunManualSetClearsOtherMain()
        {
            PlayMainCameraTestScope scope;
            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("cam-manual");
            const std::shared_ptr<GameObject> goA = scene->CreateGameObject();
            const std::shared_ptr<GameObject> goB = scene->CreateGameObject();
            const std::shared_ptr<CameraComponent> camA = goA->AddComponent<CameraComponent>();
            const std::shared_ptr<CameraComponent> camB = goB->AddComponent<CameraComponent>();

            scene->DispatchBeginPlay();
            camA->SetSelfAsMainCamera();
            camB->SetSelfAsMainCamera();
            if (camA->IsMainCamera() || !camB->IsMainCamera())
            {
                ME_LOG(LogTest, Error, "PlayMainCamera: SetSelfAsMainCamera should clear previous Main.");
                return false;
            }
            return true;
        }
    }

    bool RunPlayMainCameraTests()
    {
        return RunSoleCameraAutoMainOnBeginPlay()
            && RunBoolForcesMainAmongMany()
            && RunManualSetClearsOtherMain();
    }
}

#include "doctest.h"
#include "EngineTestFixture.h"

TEST_CASE("play-main-camera: beginplay sole bool manual [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunPlayMainCameraTests());
}
