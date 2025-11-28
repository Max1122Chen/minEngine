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

        Matrix4 GetViewMatrix() const;
        Matrix4 GetProjectionMatrix() const;

    public:
        Vector3 m_CameraVelocity;
    
    // private:
        Vector3 m_Position;
        Vector3 m_Rotation;

        float m_Yaw = -90.0f;   // Initialized to -90.0 degrees to look along the negative Z axis
        float m_Pitch = 0.0f;
        // float m_Roll = 0.0f;

        Vector3 m_Target;
        Vector3 m_Forward;
        Vector3 m_Right;
        Vector3 m_Up;
        
        // Currently hardcoded projection parameters
        float m_FOV = 45.0f;
        float m_zNear = 0.1f;
        float m_zFar = 1000.0f;
        float m_AspectRatio = 16.0f / 9.0f;


    };
}