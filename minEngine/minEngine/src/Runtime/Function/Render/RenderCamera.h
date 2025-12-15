#pragma once
#include "Core.h"

#include "Runtime/Core/Math/Math.h"

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
        void SetRotation(const Vector3& rotation) { m_Rotation = rotation; }

        const Vector3& GetPosition() const { return m_Position; }
        const Vector3& GetRotation() const { return m_Rotation; }

        Matrix4 GetViewMatrix() const { return m_ViewMatrix; }
        void SetViewMatrix(const Matrix4& viewMatrix) { m_ViewMatrix = viewMatrix; }
        void UpdateViewMatrix();

        Matrix4 GetProjectionMatrix() const { return m_ProjectionMatrix; }
        void SetProjectionMatrix(const Matrix4& projectionMatrix) { m_ProjectionMatrix = projectionMatrix; }
        void UpdateProjectionMatrix();

    public:
    
    // private:
        Matrix4 m_ViewMatrix;
        Matrix4 m_ProjectionMatrix;

        Vector3 m_Position;
        Vector3 m_Rotation;
        
        // Currently hardcoded projection parameters
        float m_FOV = 45.0f;
        float m_zNear = 0.1f;
        float m_zFar = 1000.0f;
        float m_AspectRatio = 16.0f / 9.0f;


    };
}