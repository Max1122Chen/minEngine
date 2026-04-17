#include "Viewport/EditorViewportClient.h"

#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Runtime/Function/Input/InputSystem.h"

#include <algorithm>

namespace minEngine
{
    EditorViewportClient::EditorViewportClient(std::string debugName)
        : m_DebugName(std::move(debugName))
    {
    }

    void EditorViewportClient::BeginFrame(float deltaTime)
    {
        m_LastDeltaTime = deltaTime;
    }

    void EditorViewportClient::UpdateFrameState(const ViewportFrameState& frameState)
    {
        m_FrameState = frameState;
    }

    void EditorViewportClient::EndFrame()
    {
        InputKeys();
        ExecuteInputCommands();
        SyncRenderTargetSize();
    }

    void EditorViewportClient::InputKeys()
    {
        m_PendingInputCommands.clear();

        // Viewport input is only active while this viewport is focused.
        if (!IsFocused())
        {
            if (m_IsNavigating)
            {
                SetNavigating(false);
            }
            return;
        }

        if (!m_DefaultInputBindingsRegistered)
        {
            RegisterDefaultInputBindings();
        }

        EvaluateInputBindings();
    }

    std::vector<ViewportInputCommand> EditorViewportClient::ConsumePendingInputCommands()
    {
        std::vector<ViewportInputCommand> commands;
        commands.swap(m_PendingInputCommands);
        return commands;
    }

    void EditorViewportClient::RegisterDefaultInputBindings()
    {
        if (m_DefaultInputBindingsRegistered)
        {
            return;
        }

        m_InputBindings = {
            { InputKeys::Mouse_Right, ViewportInputTriggerType::Pressed,  ViewportInputCommandType::BeginNavigate },
            { InputKeys::Mouse_Right, ViewportInputTriggerType::Released, ViewportInputCommandType::EndNavigate },
            { InputKeys::Key_W,       ViewportInputTriggerType::Down,     ViewportInputCommandType::MoveForward },
            { InputKeys::Key_S,       ViewportInputTriggerType::Down,     ViewportInputCommandType::MoveBackward },
            { InputKeys::Key_A,       ViewportInputTriggerType::Down,     ViewportInputCommandType::MoveLeft },
            { InputKeys::Key_D,       ViewportInputTriggerType::Down,     ViewportInputCommandType::MoveRight },
            { InputKeys::Key_E,       ViewportInputTriggerType::Down,     ViewportInputCommandType::MoveUp },
            { InputKeys::Key_Q,       ViewportInputTriggerType::Down,     ViewportInputCommandType::MoveDown },
            { InputKeys::Key_LeftShift, ViewportInputTriggerType::Down,   ViewportInputCommandType::SpeedBoost },
            { InputKeys::MouseScroll, ViewportInputTriggerType::Down,     ViewportInputCommandType::AdjustMoveSpeed },
            { InputKeys::Key_F,       ViewportInputTriggerType::Pressed,  ViewportInputCommandType::FocusSelection },
            { InputKeys::Key_Escape,  ViewportInputTriggerType::Pressed,  ViewportInputCommandType::Cancel }
        };

        m_DefaultInputBindingsRegistered = true;
    }

    void EditorViewportClient::EvaluateInputBindings()
    {
        for (const ViewportInputBinding& binding : m_InputBindings)
        {
            if (!IsBindingTriggered(binding))
            {
                continue;
            }

            EmitInputCommand(binding.Command, binding.Trigger);
        }
    }

    bool EditorViewportClient::IsBindingTriggered(const ViewportInputBinding& binding) const
    {
        switch (binding.Trigger)
        {
        case ViewportInputTriggerType::Pressed:
            return InputSystem::KeyPressed(binding.Key);
        case ViewportInputTriggerType::Released:
            return InputSystem::KeyReleased(binding.Key);
        case ViewportInputTriggerType::Down:
            return InputSystem::KeyDown(binding.Key);
        default:
            return false;
        }
    }

    void EditorViewportClient::EmitInputCommand(ViewportInputCommandType commandType,
                                                ViewportInputTriggerType triggerType)
    {
        m_PendingInputCommands.push_back({ commandType, triggerType });
    }

    void EditorViewportClient::ExecuteInputCommands()
    {
        const auto& renderSystem = RuntimeGlobalContext::GetRuntimeGlobalContext().m_RenderSystem;
        if (!renderSystem)
        {
            return;
        }

        RenderCamera* mainCamera = renderSystem->GetMainCamera();
        if (!mainCamera)
        {
            return;
        }

        EnsureCameraStateInitialized(*mainCamera);

        std::vector<ViewportInputCommand> commands = ConsumePendingInputCommands();
        bool requestBeginNavigate = false;
        bool requestEndNavigate = false;
        bool requestCancel = false;
        bool speedBoostEnabled = false;
        bool hasSpeedAdjustment = false;

        for (const ViewportInputCommand& command : commands)
        {
            switch (command.Type)
            {
            case ViewportInputCommandType::BeginNavigate:
                requestBeginNavigate = true;
                break;
            case ViewportInputCommandType::EndNavigate:
                requestEndNavigate = true;
                break;
            case ViewportInputCommandType::Cancel:
                requestCancel = true;
                break;
            case ViewportInputCommandType::SpeedBoost:
                speedBoostEnabled = true;
                break;
            case ViewportInputCommandType::AdjustMoveSpeed:
                hasSpeedAdjustment = true;
                break;
            default:
                break;
            }
        }

        if (requestEndNavigate || requestCancel)
        {
            SetNavigating(false);
        }

        if (requestBeginNavigate && IsHovered())
        {
            SetNavigating(true);
        }

        if (hasSpeedAdjustment)
        {
            ApplyMoveSpeedFromScroll();
        }

        if (!m_IsNavigating)
        {
            SyncStateFromRenderCamera(*mainCamera);
            return;
        }

        bool stateChanged = false;
        stateChanged |= ApplyLookFromMouse();
        stateChanged |= ApplyMovementFromCommands(commands, speedBoostEnabled);
        if (stateChanged)
        {
            ApplyStateToRenderCamera(*mainCamera);
        }
    }

    void EditorViewportClient::EnsureCameraStateInitialized(const RenderCamera& camera)
    {
        if (m_CameraStateInitialized)
        {
            return;
        }

        SyncStateFromRenderCamera(camera);
        m_CameraStateInitialized = true;
    }

    void EditorViewportClient::SyncStateFromRenderCamera(const RenderCamera& camera)
    {
        m_CameraPosition = camera.GetPosition();
        m_CameraRotation = camera.GetRotation();
    }

    void EditorViewportClient::ApplyStateToRenderCamera(RenderCamera& camera)
    {
        camera.SetPosition(m_CameraPosition);
        camera.SetRotation(m_CameraRotation);
        camera.UpdateViewMatrix();
        camera.UpdateViewProjMatrix();
    }

    void EditorViewportClient::SetNavigating(bool navigating)
    {
        if (m_IsNavigating == navigating)
        {
            return;
        }

        m_IsNavigating = navigating;
        m_HasLastMousePositionSample = false;

        const auto& windowSystem = RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem;
        if (windowSystem)
        {
            windowSystem->SetCursorVisible(!m_IsNavigating);
        }
    }

    bool EditorViewportClient::ApplyLookFromMouse()
    {
        const Vector2 mousePosition = InputSystem::GetMousePosition();
        if (!m_HasLastMousePositionSample)
        {
            m_LastMousePosition = mousePosition;
            m_HasLastMousePositionSample = true;
            return false;
        }

        const Vector2 mouseDelta = mousePosition - m_LastMousePosition;
        m_LastMousePosition = mousePosition;

        if (Math::abs(mouseDelta.x) < 0.0001f && Math::abs(mouseDelta.y) < 0.0001f)
        {
            return false;
        }

        m_CameraRotation.y -= mouseDelta.x * m_MouseSensitivity;
        m_CameraRotation.z -= mouseDelta.y * m_MouseSensitivity;
        m_CameraRotation.z = std::clamp(m_CameraRotation.z, m_MinPitch, m_MaxPitch);
        return true;
    }

    bool EditorViewportClient::ApplyMovementFromCommands(const std::vector<ViewportInputCommand>& commands,
                                                         bool speedBoostEnabled)
    {
        int moveForwardAxis = 0;
        int moveRightAxis = 0;
        int moveUpAxis = 0;
        for (const ViewportInputCommand& command : commands)
        {
            switch (command.Type)
            {
            case ViewportInputCommandType::MoveForward:
                ++moveForwardAxis;
                break;
            case ViewportInputCommandType::MoveBackward:
                --moveForwardAxis;
                break;
            case ViewportInputCommandType::MoveRight:
                ++moveRightAxis;
                break;
            case ViewportInputCommandType::MoveLeft:
                --moveRightAxis;
                break;
            case ViewportInputCommandType::MoveUp:
                ++moveUpAxis;
                break;
            case ViewportInputCommandType::MoveDown:
                --moveUpAxis;
                break;
            default:
                break;
            }
        }

        Vector3 moveDirection = Vector3(0.0f, 0.0f, 0.0f);

        Matrix4 rotationMatrix = Matrix4(1.0f);
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_CameraRotation.x), Vector3(1.0f, 0.0f, 0.0f));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_CameraRotation.y), Vector3(0.0f, 1.0f, 0.0f));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_CameraRotation.z), Vector3(0.0f, 0.0f, 1.0f));
        const Vector3 forward = glm::normalize(Vector3(rotationMatrix * Vector4(1.0f, 0.0f, 0.0f, 0.0f)));

        const Vector3 worldUp = Vector3(0.0f, 1.0f, 0.0f);
        Vector3 right = glm::cross(forward, worldUp);
        if (glm::dot(right, right) < 0.000001f)
        {
            right = Vector3(0.0f, 0.0f, 1.0f);
        }
        else
        {
            right = glm::normalize(right);
        }

        moveDirection += forward * static_cast<float>(moveForwardAxis);
        moveDirection += right * static_cast<float>(moveRightAxis);
        moveDirection += worldUp * static_cast<float>(moveUpAxis);

        if (glm::dot(moveDirection, moveDirection) < 0.000001f)
        {
            return false;
        }

        moveDirection = glm::normalize(moveDirection);
        const float speedScale = speedBoostEnabled ? m_BoostMultiplier : 1.0f;
        const float step = std::max(m_LastDeltaTime, 0.0f) * m_MoveSpeed * speedScale;
        m_CameraPosition += moveDirection * step;
        return true;
    }

    void EditorViewportClient::ApplyMoveSpeedFromScroll()
    {
        const Vector2 scrollDelta = InputSystem::GetMouseScrollDelta();
        if (Math::abs(scrollDelta.y) < 0.0001f)
        {
            return;
        }

        m_MoveSpeed += scrollDelta.y * m_MoveSpeedStep;
        m_MoveSpeed = std::clamp(m_MoveSpeed, m_MoveSpeedMin, m_MoveSpeedMax);
    }

    void EditorViewportClient::SyncRenderTargetSize()
    {
        const auto& renderSystem = RuntimeGlobalContext::GetRuntimeGlobalContext().m_RenderSystem;
        if (!renderSystem)
        {
            return;
        }

        const uint32_t targetWidth = m_FrameState.ImageSize.x > 0 ? static_cast<uint32_t>(m_FrameState.ImageSize.x) : 1;
        const uint32_t targetHeight = m_FrameState.ImageSize.y > 0 ? static_cast<uint32_t>(m_FrameState.ImageSize.y) : 1;
        if (targetWidth == m_LastRequestedWidth && targetHeight == m_LastRequestedHeight)
        {
            return;
        }

        renderSystem->RequestSceneViewportResize(targetWidth, targetHeight);
        m_LastRequestedWidth = targetWidth;
        m_LastRequestedHeight = targetHeight;
    }
}
