#include "CameraComponent.h"

#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/RenderSystem.h"

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

        // Set this camera as the main camera in the RenderSystem
        MINENGINE_ASSERT(m_RenderCamera, "RenderCamera is null!");
        RenderSystem::GetRenderSystem().SetMainCamera(m_RenderCamera);
        m_bIsMainCamera = true;
    }

    void CameraComponent::DoEndOfFrameUpdate()
    {
        if(m_bRenderStateDirty)
        {
            // Update RenderCamera's View and Projection Matrices based on CameraComponent's Transform
            MINENGINE_ASSERT(m_RenderCamera, "RenderCamera is null!");
            
            m_RenderCamera->SetPosition(GetPosition());
            m_RenderCamera->SetRotation(GetRotation());

            m_RenderCamera->UpdateViewMatrix();

            
        }
        m_bRenderStateDirty = false;
    }
}
