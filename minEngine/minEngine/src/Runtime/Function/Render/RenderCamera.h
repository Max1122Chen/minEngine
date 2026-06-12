#pragma once
#include "Core.h"

#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Math/Quaternion.h"
#include "Math/Geometry/Ray.h"

namespace minEngine
{
    class RenderCamera
    {
    public:
        friend class RenderSystem;

        RenderCamera() = default;

        ~RenderCamera() = default;

        void Initialize();

        void SetPosition(const Vector3& position) { m_Position = position; }
        const Vector3& GetPosition() const { return m_Position; }

        void SetRotation(const Quaternion& rotation) { m_Rotation = Quaternion::FromGlm(glm::normalize(rotation.ToGlm())); }
        const Quaternion& GetRotation() const { return m_Rotation; }
        Vector3 GetRotationEulerDegrees() const { return m_Rotation.ToEulerDegreesXYZ(); }
        void SetRotationEulerDegrees(const Vector3& rotationEulerDegrees)
        {
            m_Rotation = Quaternion::FromEulerDegreesXYZ(rotationEulerDegrees);
        }

        Matrix4 GetViewMatrix() const { return m_ViewMatrix; }
        void SetViewMatrix(const Matrix4& viewMatrix) { m_ViewMatrix = viewMatrix; }
        void UpdateViewMatrix();

        Matrix4 GetProjectionMatrix() const { return m_ProjectionMatrix; }
        void SetProjectionMatrix(const Matrix4& projectionMatrix) { m_ProjectionMatrix = projectionMatrix; }
        void UpdateProjectionMatrix();

        Matrix4 GetViewProjMatrix() const { return m_ViewProjMatrix; }
        void SetViewProjMatrix(const Matrix4& viewProjMatrix) { m_ViewProjMatrix = viewProjMatrix; }
        void UpdateViewProjMatrix();

        Geometry::Ray ScreenPointToRay(const Vector2& screenPoint, const Vector2& bufferSize) const;
    public:
    
    // private:
        Matrix4 m_ViewMatrix;
        Matrix4 m_ProjectionMatrix;
        Matrix4 m_ViewProjMatrix;

        Vector3 m_Position;
        Quaternion m_Rotation;
        
        // Currently hardcoded projection parameters
        float m_FOV = 45.0f;
        float m_zNear = 1.0f;
        float m_zFar = 1000.0f;
        float m_AspectRatio = 16.0f / 9.0f;


    };
}
