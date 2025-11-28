#include "InputSystem.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RenderCamera.h"

#include "glfw/glfw3.h"
#include "Runtime/Core/Math/Math.h"
#include "glm/glm.hpp"


namespace minEngine
{
    void InputSystem::Initialize()
    {
        WindowSystem* windowSystem = RuntimeGlobalContext::GetInstance().m_WindowSystem.get();

        // TODO: maybe we will wrap these logic into a private function later
        windowSystem->RegisterOnKeyCallback([this](int key, int scancode, int action, int mods)
        {
            this->OnKey(key, scancode, action, mods);
        });

        windowSystem->RegisterOnCursorPosCallback([this](double xPos, double yPos)
        {
            this->OnCursorPos(xPos, yPos);
        });
    
        ME_CORE_INFO("InputSystem Initialized"); 
    }

    void InputSystem::Shutdown()
    {
        
    }

    void InputSystem::OnKey(int key, int scancode, int action, int mods)
    {
        // just for testing
        RuntimeGlobalContext& context = RuntimeGlobalContext::GetInstance();
        RenderSystem& renderSystem = *context.m_RenderSystem;
        RenderCamera& camera = *renderSystem.GetMainCamera();

        // TODO: remove dependency on GLFW key codes later
        if(action == GLFW_PRESS)
        {
            switch(key)
            {
                case GLFW_KEY_ESCAPE:
                    context.m_WindowSystem->Close();
                    break;
                case GLFW_KEY_W:
                    camera.m_CameraVelocity.z = 1.0f;
                    break;
                case GLFW_KEY_S:
                    camera.m_CameraVelocity.z = -1.0f;
                    break;
                case GLFW_KEY_A:
                    camera.m_CameraVelocity.x = -1.0f;
                    break;
                case GLFW_KEY_D:
                    camera.m_CameraVelocity.x = 1.0f;
                    break;
                case GLFW_KEY_Q:
                    camera.m_CameraVelocity.y = 1.0f;
                    break;
                case GLFW_KEY_E:
                    camera.m_CameraVelocity.y = -1.0f;
                    break;
                default:
                    break;
            }
        }
        else if(action == GLFW_RELEASE)
        {
            switch(key)
            {
                case GLFW_KEY_W:
                case GLFW_KEY_S:
                    camera.m_CameraVelocity.z = 0.0f;
                    break;
                case GLFW_KEY_A:
                case GLFW_KEY_D:
                    camera.m_CameraVelocity.x = 0.0f;
                    break;
                case GLFW_KEY_Q:
                case GLFW_KEY_E:
                    camera.m_CameraVelocity.y = 0.0f;
                    break;
                default:
                    break;
            }
        }
    }

    void InputSystem::OnCursorPos(double xPos, double yPos)
    {
        RuntimeGlobalContext& context = RuntimeGlobalContext::GetInstance();
        RenderSystem& renderSystem = *context.m_RenderSystem;
        RenderCamera& camera = *renderSystem.GetMainCamera();

        double xOffset = xPos - m_LastCursorX;
        double yOffset = m_LastCursorY - yPos; // Reversed since y-coordinates go from bottom to top

        m_LastCursorX = static_cast<int>(xPos);
        m_LastCursorY = static_cast<int>(yPos);

        xOffset *= m_CursorSensitivity;
        yOffset *= m_CursorSensitivity;

        camera.m_Yaw += static_cast<float>(xOffset);
        camera.m_Pitch += static_cast<float>(yOffset);

        // Constrain the pitch
        if (camera.m_Pitch > 89.0f)
            camera.m_Pitch = 89.0f; 
        if (camera.m_Pitch < -89.0f)
            camera.m_Pitch = -89.0f;
        
        // Update camera front vector
        Vector3 front;
        front.x = cos(glm::radians(camera.m_Yaw)) * cos(glm::radians(camera.m_Pitch));
        front.y = sin(glm::radians(camera.m_Pitch));
        front.z = sin(glm::radians(camera.m_Yaw)) * cos(glm::radians(camera.m_Pitch));
        camera.m_Forward = glm::normalize(front);
        camera.m_Right = glm::normalize(glm::cross(camera.m_Forward, Vector3(0.0f, 1.0f, 0.0f)));
        camera.m_Up = glm::normalize(glm::cross(camera.m_Right, camera.m_Forward));
    }
}