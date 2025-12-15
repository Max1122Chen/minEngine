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

            // Update View Matrix and cache it in RenderCamera to avoid redundant calculations
            Matrix4 viewMatrix = Matrix4(1.0f);
            viewMatrix = glm::lookAt(m_RenderCamera->m_Position, m_RenderCamera->m_Position + m_RenderCamera->m_Forward, m_RenderCamera->m_Up);
            m_RenderCamera->SetViewMatrix(viewMatrix);

            // we assume only transform changes will happen now

            // Update Projection Matrix and cache it in RenderCamera to avoid redundant calculations
            // Matrix4 projectionMatrix = Matrix4(1.0f);
            // projectionMatrix = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_zNear, m_zFar);
            // m_RenderCamera->SetProjectionMatrix(projectionMatrix);

            
        }
        m_bRenderStateDirty = false;
    }
}
