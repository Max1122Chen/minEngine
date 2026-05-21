#include "CameraComponent.h"

#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/SceneViewport.h"
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

    void CameraComponent::SetSelfAsMainCamera()
    {
        if(m_bIsMainCamera)
        {
            return;
        }

        ME_ASSERT(m_RenderCamera, "RenderCamera is null!");
        if (SceneViewport* editorViewport = SceneManager::Get().GetEditorSceneViewport())
        {
            editorViewport->SetActiveCamera(m_RenderCamera);
        }
        m_bIsMainCamera = true;
    }

    void CameraComponent::DoEndOfFrameUpdate()
    {
        if(m_bRenderStateDirty)
        {
            // Update RenderCamera's View and Projection Matrices based on CameraComponent's Transform
            ME_ASSERT(m_RenderCamera, "RenderCamera is null!");
            
            m_RenderCamera->SetPosition(GetPosition());
            m_RenderCamera->SetRotation(GetRotation());

            m_RenderCamera->UpdateViewMatrix();

            
        }
        m_bRenderStateDirty = false;
    }
}
