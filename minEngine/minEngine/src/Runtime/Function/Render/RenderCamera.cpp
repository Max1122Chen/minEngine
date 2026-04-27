#include "RenderCamera.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderSystem.h"
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
        // Calculate Logic Forward Vector based on Rotation
        Matrix4 rotationMatrix = glm::mat4(1.0f);
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_Rotation.x), Vector3(1.0f, 0.0f, 0.0f));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_Rotation.y), Vector3(0.0f, 1.0f, 0.0f));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_Rotation.z), Vector3(0.0f, 0.0f, 1.0f));

        Vector3 logicForward = glm::normalize(Vector3(rotationMatrix * Vector4(1.0f, 0.0f, 0.0f, 0.0f)));

        // Convert Logic Forward to Render Forward
        // Logic: X=Forward, Y=Up, Z=Right
        // Render: -Z=Forward, Y=Up, X=Right
        // Mapping: Logic(x,y,z) -> Render(z, y, -x)
        Vector3 renderForward = Vector3(logicForward.z, logicForward.y, -logicForward.x);

        Vector3 renderingPosition = Vector3(m_Position.z, m_Position.y, -m_Position.x);
        m_ViewMatrix = glm::lookAt(renderingPosition, renderingPosition + renderForward, Vector3(0.0f, 1.0f, 0.0f));
    }

    void RenderCamera::UpdateProjectionMatrix()
    {
        m_ProjectionMatrix = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_zNear, m_zFar);
    }

    void RenderCamera::UpdateViewProjMatrix()
    {
        m_ViewProjMatrix = m_ProjectionMatrix * m_ViewMatrix;
    }

    Geometry::Ray RenderCamera::ScreenPointToRay(const Vector2 &screenPoint) const
    {
        Vector2 sceneBufferSize = RuntimeGlobalContext::Get().m_RenderSystem->GetSceneBufferSize();
        if (sceneBufferSize.x <= 0.0f || sceneBufferSize.y <= 0.0f)
        {
            return Geometry::Ray(m_Position, Vector3(1.0f, 0.0f, 0.0f));
        }

        // Input contract: screenPoint is in top-left-origin pixel space.
        const float ndcX = (screenPoint.x / sceneBufferSize.x) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (screenPoint.y / sceneBufferSize.y) * 2.0f;

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