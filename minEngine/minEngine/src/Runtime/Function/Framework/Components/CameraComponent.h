#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"

namespace minEngine
{
    class RenderCamera;

    ME_CLASS()
    class CameraComponent : public SceneComponent
    {
    public:
        CameraComponent();
        virtual ~CameraComponent();    

        RenderCamera* GetRenderCamera() const { return m_RenderCamera.get(); }

        void SetSelfAsMainCamera();
        bool IsMainCamera() const { return m_bIsMainCamera; }
        

        virtual void DoEndOfFrameUpdate() override;

    protected:
        std::shared_ptr<RenderCamera> m_RenderCamera;
        bool m_bIsMainCamera{ false };

        float m_FOV{ 45.0f };
        float m_AspectRatio{ 16.0f / 9.0f };
        float m_zNear{ 0.1f };
        float m_zFar{ 1000.0f };

        float m_Yaw{ -90.0f };   // Initialized to -90.0 degrees to look along the negative Z axis
        float m_Pitch{ 0.0f };

        Vector3 m_Forward;
        Vector3 m_Right;
        Vector3 m_Up;

    };
}

#include "Generated/Reflection/CameraComponent.gen.h"
