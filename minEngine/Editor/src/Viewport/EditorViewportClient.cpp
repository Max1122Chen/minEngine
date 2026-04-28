#include "EditorViewportClient.h"

#include "Math/Math.h"
#include "Math/Geometry/Ray.h"
#include "Math/Geometry/AABB.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Render/RenderSystem.h"
#include "Render/RenderCamera.h"
#include "Render/WindowSystem.h"
#include "Render/RenderScene.h"
#include "Runtime/Function/Input/InputSystem.h"

#include "Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"
#include "Render/PrimitiveSceneProxies/StaticMeshSceneProxy.h"
#include "Function/Framework/Components/PrimitiveComponent.h"
#include "Function/Framework/Components/StaticMeshComponent.h"
#include "Render/StaticMesh.h"
#include "Function/Framework/GameObject/GameObject.h"
#include "Editor.h"

#include <algorithm>
#include <limits>


namespace minEngine
{
    EditorViewportClient::EditorViewportClient(std::string debugName)
        : m_DebugName(std::move(debugName))
    {
        m_GizmoState.mode = GizmoState::Mode::Translate;
    }

    void EditorViewportClient::SetInputBlockedByGizmo(bool blocked)
    {
        m_InputBlockedByGizmo = blocked;
    }

    void EditorViewportClient::BeginFrame(float deltaTime)
    {
        m_LastDeltaTime = deltaTime;
        // Clear Gizmo manipulation state at the beginning of each frame but not the mode
        m_GizmoState.Using = false;
        m_GizmoState.Hovering = false;
        m_GizmoState.Manipulated = false;
        m_GizmoState.axis = GizmoState::Axis::None;
        m_GizmoState.Delta.Reset();
    }

    void EditorViewportClient::UpdateFrameState(const ViewportFrameState& frameState)
    {
        m_FrameState = frameState;
    }

    void EditorViewportClient::EndFrame()
    {
        ConsumeGizmoManipulation();
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
            // Navigation
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
            { InputKeys::Key_Escape,  ViewportInputTriggerType::Pressed,  ViewportInputCommandType::Cancel },

            // Gizmo Mode Switching
            { InputKeys::Key_W,       ViewportInputTriggerType::Pressed,  ViewportInputCommandType::SetGizmoModeTranslate },
            { InputKeys::Key_E,       ViewportInputTriggerType::Pressed,  ViewportInputCommandType::SetGizmoModeRotate },
            { InputKeys::Key_R,       ViewportInputTriggerType::Pressed,  ViewportInputCommandType::SetGizmoModeScale },

            // GameObject Selection
            { InputKeys::Mouse_Left,  ViewportInputTriggerType::Pressed,  ViewportInputCommandType::Select }
    
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
        const auto& renderSystem = RuntimeGlobalContext::Get().m_RenderSystem;
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
        GizmoState::Mode requestedGizmoMode = GizmoState::Mode::None;
        bool requestSelection = false;

        for (const ViewportInputCommand& command : commands)
        {
            switch (command.Type)
            {
            // Navigation
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
            // Gizmo Mode Switching
            case ViewportInputCommandType::SetGizmoModeTranslate:
                requestedGizmoMode = GizmoState::Mode::Translate;
                break;
            case ViewportInputCommandType::SetGizmoModeRotate:
                requestedGizmoMode = GizmoState::Mode::Rotate;
                break;
            case ViewportInputCommandType::SetGizmoModeScale:
                requestedGizmoMode = GizmoState::Mode::Scale;
                break;
            // GameObject Selection
            case ViewportInputCommandType::Select:
                requestSelection = true;
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

        if (requestSelection && IsHovered() && IsFocused() && !m_IsNavigating && !m_InputBlockedByGizmo)
        {
            TrySelectAtMousePosition();
        }

        if (requestedGizmoMode != GizmoState::Mode::None && m_GizmoState.mode != requestedGizmoMode && IsHovered() && IsFocused() && !m_IsNavigating)
        {
            // Update Gizmo mode
            m_GizmoState.mode = requestedGizmoMode;
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

        const auto& windowSystem = RuntimeGlobalContext::Get().m_WindowSystem;
        if (windowSystem)
        {
            windowSystem->SetCursorVisible(!m_IsNavigating);
        }
    }

    void EditorViewportClient::ConsumeGizmoManipulation()
    {
        //Consume the Gizmo's new transform and apply it to the selected GameObject if there is any first in case new GO is going to be selected this frame.
        if (m_GizmoState.Manipulated)
        {
            if (GameObject* selected = m_Editor->GetSelectedGameObject())
            {
                switch(m_GizmoState.mode)
                {
                case GizmoState::Mode::Translate:
                {   
                    m_Editor->GetSelectedGameObject()->Translate(m_GizmoState.Delta.PositionDelta); 
                    break;
                }
                case GizmoState::Mode::Rotate:    
                {
                    m_Editor->GetSelectedGameObject()->Rotate(m_GizmoState.Delta.RotationDelta, Space::World);
                    break;
                }
                case GizmoState::Mode::Scale:
                {
                    m_Editor->GetSelectedGameObject()->ScaleBy(m_GizmoState.Delta.ScaleDelta);
                    break;
                }
                default: break;
                }
            }
        }
    }

    void EditorViewportClient::TrySelectAtMousePosition()
    {
        RuntimeGlobalContext& context = RuntimeGlobalContext::Get();
        if (!context.m_RenderSystem)
        {
            return;
        }

        Vector2 mousePosition = InputSystem::GetMousePosition();
        mousePosition -= m_FrameState.ImageMin;

        Vector2 viewportSize = m_FrameState.ImageSize;
        if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
        {
            return;
        }

        Vector2 sceneBufferSize = context.m_RenderSystem->GetSceneBufferSize();
        if (sceneBufferSize.x <= 0.0f || sceneBufferSize.y <= 0.0f)
        {
            return;
        }

        float xRatio = sceneBufferSize.x / viewportSize.x;
        float yRatio = sceneBufferSize.y / viewportSize.y;
        Vector2 scaledMousePosition = mousePosition * Vector2(xRatio, yRatio);

        RenderCamera* mainCamera = context.m_RenderSystem->GetMainCamera();
        if (!mainCamera)
        {
            return;
        }

        Geometry::Ray pickRay = mainCamera->ScreenPointToRay(scaledMousePosition);

        RenderScene* scene = context.m_RenderSystem->m_RenderScene.get();
        if (!scene)
        {
            return;
        }

        float closestHitDistance = std::numeric_limits<float>::max();
        GameObject* closestHitObject = nullptr;
        for (const PrimitiveSceneProxy* proxy : scene->m_PrimitiveSceneProxies)
        {
            if (!proxy)
            {
                continue;
            }

            PrimitiveComponent* primitiveComponent = proxy->m_PrimitiveComponent;
            if (!primitiveComponent)
            {
                continue;
            }

            StaticMeshComponent* staticMeshComponent = dynamic_cast<StaticMeshComponent*>(primitiveComponent);
            if (!staticMeshComponent)
            {
                continue;
            }

            StaticMesh* staticMesh = staticMeshComponent->GetMesh();
            if (!staticMesh)
            {
                continue;
            }

            Geometry::AABB boundingBox = staticMesh->m_BoundingBox;

            Geometry::AABB worldBoundingBox = Geometry::Transform(boundingBox, staticMeshComponent->GetTransform().ToMatrix());
            float distance = std::numeric_limits<float>::max();
            bool intersected = worldBoundingBox.IntersectRay(pickRay, distance);
            if (intersected && distance < closestHitDistance)
            {
                closestHitDistance = distance;
                closestHitObject = staticMeshComponent->GetOwner();
            }
        }

        if (closestHitObject)
        {
            m_Editor->SelectGameObject(closestHitObject->GetID());
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
        const auto& renderSystem = RuntimeGlobalContext::Get().m_RenderSystem;
        if (!renderSystem)
        {
            return;
        }

        // Calculate the delta ratio between the requested size and the last requested size, and request the render system to resize the scene viewport accordingly.
        // Because the actual render target resolution is not necessarily the same as the viewport's content size, we use the delta ratio to ensure the render target size can track the viewport size changes in a more consistent way.
        // For example, the render target may have resolution of 1920 * 1080 while the viewport content size is 1280 * 720, and later the viewport content size changes to 1600 * 900. In this case, we want the render target to be resized to 1920 * 1080 * (1600/1280) * (900/720) = 2400 * 1350, instead of just resized to 1600 * 900.
        uint32_t requestedWidth = m_FrameState.ImageSize.x > 0 ? static_cast<uint32_t>(m_FrameState.ImageSize.x) : 1;
        uint32_t requestedHeight = m_FrameState.ImageSize.y > 0 ? static_cast<uint32_t>(m_FrameState.ImageSize.y) : 1;
        const float targetWidthRatio = static_cast<float>(requestedWidth) / static_cast<float>(m_LastRequestedWidth);
        const float targetHeightRatio = static_cast<float>(requestedHeight) / static_cast<float>(m_LastRequestedHeight);

        renderSystem->RequestSceneViewportResize(targetWidthRatio, targetHeightRatio);
        m_LastRequestedWidth = requestedWidth;
        m_LastRequestedHeight = requestedHeight;
    }
}
