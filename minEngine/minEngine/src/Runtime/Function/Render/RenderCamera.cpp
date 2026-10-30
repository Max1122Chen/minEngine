#include "RenderCamera.h"

#include "Render/RHI/RHIClipSpace.h"
#include "Render/RHI/RHIClipSpaceCapabilities.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace minEngine
{
    namespace
    {
        constexpr float kMinPositiveLens = 1.0e-4f;
    }

    void RenderCamera::Initialize()
    {
        UpdateViewMatrix();
        UpdateProjectionMatrix();
        UpdateViewProjMatrix();
    }

    Vector3 RenderCamera::GetForwardDirection() const
    {
        const glm::quat rotationQuat = m_Rotation.ToGlm();
        return glm::normalize(rotationQuat * Vector3(1.0f, 0.0f, 0.0f));
    }

    void RenderCamera::UpdateViewMatrix()
    {
        const Vector3 forward = GetForwardDirection();
        const Vector3 up = Math::abs(forward.y) > 0.999f ? Vector3(0.0f, 0.0f, 1.0f) : Vector3(0.0f, 1.0f, 0.0f);
        m_ViewMatrix = glm::lookAt(m_Position, m_Position + forward, up);
    }

    void RenderCamera::UpdateProjectionMatrix()
    {
        const float aspect = std::max(m_AspectRatio, kMinPositiveLens);

        if (m_ProjectionMode == ECameraProjectionMode::Orthographic)
        {
            const float orthoHeight = std::max(m_OrthoHeight, kMinPositiveLens);
            const float halfH = orthoHeight * 0.5f;
            const float halfW = halfH * aspect;
            m_ProjectionMatrix = RHIClipSpace::MakeOrthographic(
                -halfW,
                halfW,
                -halfH,
                halfH,
                m_zNear,
                m_zFar);
            return;
        }

        m_ProjectionMatrix = RHIClipSpace::MakePerspective(
            glm::radians(m_FOV),
            aspect,
            m_zNear,
            m_zFar);
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

        const float ndcX = (screenPoint.x / bufferSize.x) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (screenPoint.y / bufferSize.y) * 2.0f;
        const float nearNdcZ = GetFrustumNdcZNear();
        const float farNdcZ = GetFrustumNdcZFar();

        const Matrix4 invViewProj = glm::inverse(m_ViewProjMatrix);
        Vector4 nearWorld = invViewProj * Vector4(ndcX, ndcY, nearNdcZ, 1.0f);
        nearWorld /= nearWorld.w;

        Geometry::Ray outRay;
        outRay.Origin = Vector3(nearWorld);

        if (m_ProjectionMode == ECameraProjectionMode::Orthographic)
        {
            outRay.Direction = GetForwardDirection();
            return outRay;
        }

        Vector4 farWorld = invViewProj * Vector4(ndcX, ndcY, farNdcZ, 1.0f);
        farWorld /= farWorld.w;
        outRay.Direction = glm::normalize(farWorld - nearWorld);
        return outRay;
    }
}
