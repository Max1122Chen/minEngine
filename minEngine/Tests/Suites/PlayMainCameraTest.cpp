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
#include "Runtime/Function/Render/RenderCamera.h"

#include <cmath>

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

        bool RunLensParamsPushToRenderCamera()
        {
            PlayMainCameraTestScope scope;
            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("cam-lens");
            const std::shared_ptr<GameObject> go = scene->CreateGameObject();
            const std::shared_ptr<CameraComponent> camera = go->AddComponent<CameraComponent>();
            RenderCamera* renderCamera = camera->GetRenderCamera();
            if (renderCamera == nullptr)
            {
                ME_LOG(LogTest, Error, "PlayMainCamera: RenderCamera missing.");
                return false;
            }

            // Constructor ApplyLens should already leave component default (0.1), not RenderCamera ctor 1.0.
            if (renderCamera->m_zNear > 0.11f)
            {
                ME_LOG(LogTest, Error, "PlayMainCamera: ctor should push component near (0.1), got {}.",
                       renderCamera->m_zNear);
                return false;
            }

            constexpr float kVoxelNear = 0.05f;
            camera->SetNearPlane(kVoxelNear);
            camera->SetFieldOfView(70.0f);
            if (std::abs(renderCamera->m_zNear - kVoxelNear) > 0.0001f
                || std::abs(renderCamera->m_FOV - 70.0f) > 0.0001f)
            {
                ME_LOG(LogTest, Error,
                       "PlayMainCamera: SetNearPlane/SetFieldOfView must push to RenderCamera (near={} fov={}).",
                       renderCamera->m_zNear,
                       renderCamera->m_FOV);
                return false;
            }
            return true;
        }
    }

    bool RunPlayMainCameraTests()
    {
        return RunSoleCameraAutoMainOnBeginPlay()
            && RunBoolForcesMainAmongMany()
            && RunManualSetClearsOtherMain()
            && RunLensParamsPushToRenderCamera();
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
