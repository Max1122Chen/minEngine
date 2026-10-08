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
        ApplyLensToRenderCamera();
    }

    CameraComponent::~CameraComponent()
    {
    }

    void CameraComponent::SetFieldOfView(float degrees)
    {
        m_FOV = degrees;
        ApplyLensToRenderCamera();
        MarkRenderStateDirty();
    }

    void CameraComponent::SetNearPlane(float nearPlane)
    {
        m_zNear = nearPlane;
        ApplyLensToRenderCamera();
        MarkRenderStateDirty();
    }

    void CameraComponent::SetFarPlane(float farPlane)
    {
        m_zFar = farPlane;
        ApplyLensToRenderCamera();
        MarkRenderStateDirty();
    }

    void CameraComponent::ApplyLensToRenderCamera()
    {
        if (!m_RenderCamera)
        {
            return;
        }

        m_RenderCamera->m_FOV = m_FOV;
        m_RenderCamera->m_zNear = m_zNear;
        m_RenderCamera->m_zFar = m_zFar;
        // Aspect is owned by the presenting SceneViewport (buffer size); leave m_AspectRatio.
        m_RenderCamera->UpdateProjectionMatrix();
        m_RenderCamera->UpdateViewProjMatrix();
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
        ME_ASSERT(m_RenderCamera, "RenderCamera is null!");

        if (m_bRenderStateDirty)
        {
            const Transform worldTransform = GetWorldTransform();
            m_RenderCamera->SetPosition(worldTransform.Position);
            m_RenderCamera->SetRotation(worldTransform.Rotation);
            m_RenderCamera->UpdateViewMatrix();
        }

        // Keep lens params authoritative even if only FOV/near/far changed.
        ApplyLensToRenderCamera();
        m_bRenderStateDirty = false;
    }
}
