#include "RenderCamera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace minEngine
{
    void RenderCamera::Initialize()
    {
        // Initialization code for RenderCamera
        m_Position = Vector3(0.0f, 0.0f, 10.0f);
        m_Rotation = Vector3(0.0f, 0.0f, 0.0f);

        m_Target = Vector3(0.0f, 0.0f, 0.0f);
        m_Forward = glm::normalize(Vector3(0.0f, 0.0f, -1.0f));
        m_Right = glm::normalize(glm::cross(m_Forward, Vector3(0.0f, 1.0f, 0.0f)));
        m_Up = glm::normalize(glm::cross(m_Right, m_Forward));
    }

    Matrix4 RenderCamera::GetViewMatrix() const
    {
        // TODO: Implement view matrix calculation based on position and rotation
        Matrix4 viewMatrix = Matrix4(1.0f);
        viewMatrix = glm::lookAt(m_Position, m_Position + m_Forward, m_Up);

        // return glm::inverse(viewMatrix); // Don't need to invert since lookAt gives the correct view matrix
        return viewMatrix;
    }

    Matrix4 RenderCamera::GetProjectionMatrix() const
    {
        // TODO: Implement projection matrix calculation
        Matrix4 projectionMatrix = Matrix4(1.0f);

        // TODO: Figure out the principle behind glm::perspective parameters
        projectionMatrix = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_zNear, m_zFar);

        return projectionMatrix;
    }
}