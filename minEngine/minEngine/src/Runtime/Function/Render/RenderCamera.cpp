#include "RenderCamera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace minEngine
{
    void RenderCamera::Initialize()
    {
        UpdateViewMatrix();
        UpdateProjectionMatrix();
        UpdateViewProjMatrix();
    }

    void RenderCamera::UpdateViewMatrix()
    {
        // WARNING: This is a very naive implementation that directly converts Euler angles to forward vector without considering gimbal lock or other issues. It also assumes a specific order of rotation (yaw-pitch-roll).
        Matrix4 rotationMatrix = glm::mat4(1.0f);
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_Rotation.x), Vector3(1.0f, 0.0f, 0.0f));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_Rotation.y), Vector3(0.0f, 1.0f, 0.0f));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_Rotation.z), Vector3(0.0f, 0.0f, 1.0f));

        Vector3 forward = glm::normalize(Vector3(rotationMatrix * Vector4(1.0f, 0.0f, 0.0f, 0.0f)));
        Vector3 up = Math::abs(forward.y) > 0.999f ? Vector3(0.0f, 0.0f, 1.0f) : Vector3(0.0f, 1.0f, 0.0f);
        m_ViewMatrix = glm::lookAt(m_Position, m_Position + forward, up);

        // glm::quat rotationQuat = glm::quat(glm::radians(m_Rotation));
        // Vector3 forward = glm::normalize(rotationQuat * Vector3(1.0f, 0.0f, 0.0f));
        // Vector3 up = glm::normalize(rotationQuat * Vector3(0.0f, 1.0f, 0.0f));
        // m_ViewMatrix = glm::lookAt(m_Position, m_Position + forward, up);
    }

    void RenderCamera::UpdateProjectionMatrix()
    {
        m_ProjectionMatrix = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_zNear, m_zFar);
    }

    void RenderCamera::UpdateViewProjMatrix()
    {
        m_ViewProjMatrix = m_ProjectionMatrix * m_ViewMatrix;
    }

    Geometry::Ray RenderCamera::ScreenPointToRay(const Vector2& screenPoint, const Vector2& bufferSize) const
    {
        if (bufferSize.x <= 0.0f || bufferSize.y <= 0.0f)
        {
            return Geometry::Ray(m_Position, Vector3(1.0f, 0.0f, 0.0f));
        }

        // Input contract: screenPoint is in top-left-origin pixel space.
        const float ndcX = (screenPoint.x / bufferSize.x) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (screenPoint.y / bufferSize.y) * 2.0f;

        const Matrix4 invViewProj = glm::inverse(m_ViewProjMatrix);
        Vector4 nearWorld = invViewProj * Vector4(ndcX, ndcY, -1.0f, 1.0f);
        Vector4 farWorld = invViewProj * Vector4(ndcX, ndcY, 1.0f, 1.0f);
        nearWorld /= nearWorld.w;
        farWorld /= farWorld.w;

        Geometry::Ray outRay;
        outRay.Origin = Vector3(nearWorld);
        outRay.Direction = glm::normalize(farWorld - nearWorld);
        return outRay;
    }
}