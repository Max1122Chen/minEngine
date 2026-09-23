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
        ME_GENERATED_BODY()
    public:
        CameraComponent();
        virtual ~CameraComponent();    

        RenderCamera* GetRenderCamera() const { return m_RenderCamera.get(); }

        /** Mark this camera as the scene Main view camera. Clears other Mains in the owning Scene. */
        void SetSelfAsMainCamera();
        bool IsMainCamera() const { return m_bIsMainCamera; }

        void SetSetAsMainCameraOnBeginPlay(bool value) { m_bSetAsMainCameraOnBeginPlay = value; }
        bool GetSetAsMainCameraOnBeginPlay() const { return m_bSetAsMainCameraOnBeginPlay; }

        virtual void DoEndOfFrameUpdate() override;

    protected:
        void OnBeginPlay() override;

        void ClearOtherMainCamerasInScene();
        bool IsSoleActiveCameraInOwningScene() const;
        void TryPushEditViewportPreview();

        std::shared_ptr<RenderCamera> m_RenderCamera;

        bool m_bIsMainCamera{ false };

        ME_PROPERTY(EditAnywhere)
        bool m_bSetAsMainCameraOnBeginPlay{ false };

        float m_FOV{ 45.0f };
        float m_AspectRatio{ 16.0f / 9.0f };
        float m_zNear{ 0.1f };
        float m_zFar{ 1000.0f };

        float m_Yaw{ -90.0f };
        float m_Pitch{ 0.0f };

        Vector3 m_Forward;
        Vector3 m_Right;
        Vector3 m_Up;

    };
}

#include "Generated/Reflection/CameraComponent.gen.h"
