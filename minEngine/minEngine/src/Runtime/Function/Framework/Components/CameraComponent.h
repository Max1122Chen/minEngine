#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Render/RenderCamera.h"

namespace minEngine
{
    ME_CLASS()
    class MINENGINE_API CameraComponent : public SceneComponent
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

        ECameraProjectionMode GetProjectionMode() const { return m_ProjectionMode; }
        void SetProjectionMode(ECameraProjectionMode mode);

        float GetFieldOfView() const { return m_FOV; }
        void SetFieldOfView(float degrees);

        float GetOrthoHeight() const { return m_OrthoHeight; }
        void SetOrthoHeight(float height);

        float GetNearPlane() const { return m_zNear; }
        void SetNearPlane(float nearPlane);

        float GetFarPlane() const { return m_zFar; }
        void SetFarPlane(float farPlane);

        virtual void DoEndOfFrameUpdate() override;

    protected:
        void OnBeginPlay() override;

        void ClearOtherMainCamerasInScene();
        bool IsSoleActiveCameraInOwningScene() const;
        void TryPushEditViewportPreview();
        /** Push projection mode / FOV / ortho height / near / far onto m_RenderCamera. */
        void ApplyLensToRenderCamera();

        std::shared_ptr<RenderCamera> m_RenderCamera;

        bool m_bIsMainCamera{ false };

        ME_PROPERTY(EditAnywhere)
        bool m_bSetAsMainCameraOnBeginPlay{ false };

        ECameraProjectionMode m_ProjectionMode{ ECameraProjectionMode::Perspective };
        float m_FOV{ 45.0f };
        float m_OrthoHeight{ 10.0f };
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
