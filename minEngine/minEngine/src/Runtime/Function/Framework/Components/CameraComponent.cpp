#include "CameraComponent.h"

#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/SceneViewport.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

namespace minEngine
{   
    CameraComponent::CameraComponent()
    {
        m_RenderCamera = std::make_shared<RenderCamera>();
        m_RenderCamera->Initialize();
    }

    CameraComponent::~CameraComponent()
    {
    }

    void CameraComponent::ClearOtherMainCamerasInScene()
    {
        Scene* scene = GetOwningScene();
        if (scene == nullptr)
        {
            return;
        }

        for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
        {
            if (!gameObject)
            {
                continue;
            }

            for (const std::shared_ptr<CameraComponent>& camera : gameObject->GetComponentsOfType<CameraComponent>())
            {
                if (camera && camera.get() != this)
                {
                    camera->m_bIsMainCamera = false;
                }
            }
        }
    }

    bool CameraComponent::IsSoleActiveCameraInOwningScene() const
    {
        Scene* scene = GetOwningScene();
        if (scene == nullptr)
        {
            return false;
        }

        int activeCameraCount = 0;
        for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
        {
            if (!gameObject)
            {
                continue;
            }

            for (const std::shared_ptr<CameraComponent>& camera : gameObject->GetComponentsOfType<CameraComponent>())
            {
                if (camera && camera->IsActive())
                {
                    ++activeCameraCount;
                    if (activeCameraCount > 1)
                    {
                        return false;
                    }
                }
            }
        }

        return activeCameraCount == 1;
    }

    void CameraComponent::TryPushEditViewportPreview()
    {
        if (!SceneManager::HasInstance() || SceneManager::Get().IsPIEPlayActive())
        {
            return;
        }

        if (SceneViewport* editorViewport = SceneManager::Get().GetEditorSceneViewport())
        {
            editorViewport->SetActiveCamera(m_RenderCamera);
        }
    }

    void CameraComponent::SetSelfAsMainCamera()
    {
        ME_ASSERT(m_RenderCamera, "RenderCamera is null!");

        ClearOtherMainCamerasInScene();
        m_bIsMainCamera = true;
        TryPushEditViewportPreview();
    }

    void CameraComponent::OnBeginPlay()
    {
        if (m_bSetAsMainCameraOnBeginPlay || IsSoleActiveCameraInOwningScene())
        {
            SetSelfAsMainCamera();
        }
    }

    void CameraComponent::DoEndOfFrameUpdate()
    {
        if(m_bRenderStateDirty)
        {
            ME_ASSERT(m_RenderCamera, "RenderCamera is null!");
            
            const Transform worldTransform = GetWorldTransform();
            m_RenderCamera->SetPosition(worldTransform.Position);
            m_RenderCamera->SetRotation(worldTransform.Rotation);

            m_RenderCamera->UpdateViewMatrix();
        }
        m_bRenderStateDirty = false;
    }
}
