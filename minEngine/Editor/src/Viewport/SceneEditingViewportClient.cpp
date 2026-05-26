#include "SceneEditingViewportClient.h"



#include "Math/Math.h"

#include "Math/Geometry/Ray.h"

#include "Math/Geometry/AABB.h"

#include "Runtime/Function/Render/WindowSystem.h"

#include "Runtime/Function/Framework/Scene/SceneManager.h"

#include "Runtime/Function/Framework/Scene/Scene.h"

#include "Runtime/Function/Render/RenderSystem.h"

#include "Runtime/Function/Render/RHI/RHI.h"

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

#include "Shell/EditorContextHelpers.h"
#include "Scene/SceneEditor.h"
#include "Shell/EditorSubModule.h"

#include "Runtime/Function/Framework/Transform/Transform.h"

#include <algorithm>

#include <limits>



namespace minEngine

{

    SceneEditingViewportClient::SceneEditingViewportClient(std::string debugName)

        : EditorViewportClient(std::move(debugName))

    {

        m_GizmoState.mode = GizmoState::Mode::Translate;

    }



    SceneEditingViewportClient::~SceneEditingViewportClient()

    {

        if (SceneManager::Get().GetEditorSceneViewport() == &GetSceneViewport())
        {
            SceneManager::Get().SetEditorSceneViewport(nullptr);
        }
    }



    void SceneEditingViewportClient::InitializeSceneViewport(RHI* rhi, uint32_t width, uint32_t height)

    {

        if (IsSceneViewportInitialized() || !rhi)
        {
            return;
        }

        InitializeEditorSceneViewport(rhi, width, height);
        SceneManager::Get().SetEditorSceneViewport(&GetSceneViewport());

    }



    void SceneEditingViewportClient::BeginFrame(float deltaTime)

    {

        EditorViewportClient::BeginFrame(deltaTime);

        m_GizmoState.Using = false;

        m_GizmoState.Hovering = false;

        m_GizmoState.Manipulated = false;

        m_GizmoState.axis = GizmoState::Axis::None;

        m_GizmoState.Delta.Reset();

    }



    void SceneEditingViewportClient::SetInputBlockedByGizmo(bool blocked)

    {

        m_InputBlockedByGizmo = blocked;

    }



    void SceneEditingViewportClient::EndFrame()

    {
        const EditorSubModule* active = m_Context ? m_Context->GetActiveSubModule() : nullptr;
        if (!active || active->GetModuleId() != SceneEditor::kModuleId)
        {
            return;
        }

        BeginGizmoCommandSessionIfNeeded();
        ConsumeGizmoManipulation();
        EndGizmoCommandSessionIfNeeded();

        InputKeys();

        ExecuteInputCommands();

        SyncRenderTargetSize();

        SyncObservedScene();



        RHI* rhi = RenderSystem::Get().GetRHI();

        GetSceneViewport().ApplyPendingResize(rhi);



        const SceneDrawFlags flags = SceneDrawFlags::EnableShadows | SceneDrawFlags::EnablePostProcess |
                                     SceneDrawFlags::EnableSkyBox;

        const SceneDrawDesc desc = GetSceneViewport().BuildDrawDesc(flags);

        if (desc.Scene && desc.Camera && desc.RenderTarget)

        {

            RenderSystem::Get().SubmitSceneDraw(desc);

        }

    }



    void SceneEditingViewportClient::SyncObservedScene()

    {

        SceneEditor* sceneEditor = GetSceneEditor(m_Context);
        Scene* scene = sceneEditor ? sceneEditor->GetActiveScene() : nullptr;

        if (!scene)

        {

            GetSceneViewport().SetObservedScene(nullptr);

            return;

        }



        scene->EnsureRenderScene();

        GetSceneViewport().SetObservedScene(scene->GetRenderScene());

    }



    void SceneEditingViewportClient::InputKeys()

    {

        m_PendingInputCommands.clear();



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



    std::vector<ViewportInputCommand> SceneEditingViewportClient::ConsumePendingInputCommands()

    {

        std::vector<ViewportInputCommand> commands;

        commands.swap(m_PendingInputCommands);

        return commands;

    }



    void SceneEditingViewportClient::RegisterDefaultInputBindings()

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

            { InputKeys::Key_Escape,  ViewportInputTriggerType::Pressed,  ViewportInputCommandType::Cancel },

            { InputKeys::Key_W,       ViewportInputTriggerType::Pressed,  ViewportInputCommandType::SetGizmoModeTranslate },

            { InputKeys::Key_E,       ViewportInputTriggerType::Pressed,  ViewportInputCommandType::SetGizmoModeRotate },

            { InputKeys::Key_R,       ViewportInputTriggerType::Pressed,  ViewportInputCommandType::SetGizmoModeScale },

            { InputKeys::Mouse_Left,  ViewportInputTriggerType::Pressed,  ViewportInputCommandType::Select }

        };



        m_DefaultInputBindingsRegistered = true;

    }



    void SceneEditingViewportClient::EvaluateInputBindings()

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



    bool SceneEditingViewportClient::IsBindingTriggered(const ViewportInputBinding& binding) const

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



    void SceneEditingViewportClient::EmitInputCommand(ViewportInputCommandType commandType,

                                                      ViewportInputTriggerType triggerType)

    {

        m_PendingInputCommands.push_back({ commandType, triggerType });

    }



    void SceneEditingViewportClient::ExecuteInputCommands()

    {

        RenderCamera* viewportCamera = GetSceneViewport().GetCamera();

        if (!viewportCamera)

        {

            return;

        }



        EnsureCameraStateInitialized(*viewportCamera);



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

            case ViewportInputCommandType::SetGizmoModeTranslate:

                requestedGizmoMode = GizmoState::Mode::Translate;

                break;

            case ViewportInputCommandType::SetGizmoModeRotate:

                requestedGizmoMode = GizmoState::Mode::Rotate;

                break;

            case ViewportInputCommandType::SetGizmoModeScale:

                requestedGizmoMode = GizmoState::Mode::Scale;

                break;

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

            m_GizmoState.mode = requestedGizmoMode;

        }



        if (!m_IsNavigating)

        {

            SyncStateFromRenderCamera(*viewportCamera);

            return;

        }



        bool stateChanged = false;

        stateChanged |= ApplyLookFromMouse();

        stateChanged |= ApplyMovementFromCommands(commands, speedBoostEnabled);

        if (stateChanged)

        {

            ApplyStateToRenderCamera(*viewportCamera);

        }

    }



    void SceneEditingViewportClient::EnsureCameraStateInitialized(const RenderCamera& camera)

    {

        if (m_CameraStateInitialized)

        {

            return;

        }



        SyncStateFromRenderCamera(camera);

        m_CameraStateInitialized = true;

    }



    void SceneEditingViewportClient::SyncStateFromRenderCamera(const RenderCamera& camera)

    {

        m_CameraPosition = camera.GetPosition();

        m_CameraRotation = camera.GetRotation();

    }



    void SceneEditingViewportClient::ApplyStateToRenderCamera(RenderCamera& camera)

    {

        camera.SetPosition(m_CameraPosition);

        camera.SetRotation(m_CameraRotation);

        camera.UpdateViewMatrix();

        camera.UpdateViewProjMatrix();

    }



    void SceneEditingViewportClient::SetNavigating(bool navigating)

    {

        if (m_IsNavigating == navigating)

        {

            return;

        }



        m_IsNavigating = navigating;

        m_HasLastMousePositionSample = false;



        WindowSystem::Get().SetCursorVisible(!m_IsNavigating);

    }



    void SceneEditingViewportClient::ConsumeGizmoManipulation()

    {

        if (!m_GizmoState.Manipulated)

        {

            return;

        }



        SceneEditor* sceneEditor = GetSceneEditor(m_Context);
        if (GameObject* selected = sceneEditor ? sceneEditor->GetSelectedGameObject() : nullptr)

        {

            switch (m_GizmoState.mode)

            {

            case GizmoState::Mode::Translate:

                selected->Translate(m_GizmoState.Delta.PositionDelta);

                break;

            case GizmoState::Mode::Rotate:

                selected->Rotate(m_GizmoState.Delta.RotationDelta, Space::World);

                break;

            case GizmoState::Mode::Scale:

                selected->ScaleBy(m_GizmoState.Delta.ScaleDelta);

                break;

            default:

                break;

            }

        }

    }

    void SceneEditingViewportClient::BeginGizmoCommandSessionIfNeeded()
    {
        const bool usingNow = m_GizmoState.Using;
        if (!usingNow || m_GizmoWasUsing)
        {
            return;
        }

        SceneEditor* sceneEditor = GetSceneEditor(m_Context);
        if (GameObject* selected = sceneEditor ? sceneEditor->GetSelectedGameObject() : nullptr)
        {
            m_GizmoSessionGameObjectId = selected->GetID();
            m_GizmoStartTransform = selected->GetTransform();
        }
    }

    void SceneEditingViewportClient::EndGizmoCommandSessionIfNeeded()
    {
        const bool usingNow = m_GizmoState.Using;
        SceneEditor* sceneEditor = GetSceneEditor(m_Context);

        if (!usingNow && m_GizmoWasUsing && sceneEditor && m_Context)
        {
            if (m_GizmoSessionGameObjectId != std::numeric_limits<uint64_t>::max())
            {
                if (GameObject* selected = sceneEditor->GetSelectedGameObject())
                {
                    if (selected->GetID() == m_GizmoSessionGameObjectId)
                    {
                        const Transform endTransform = selected->GetTransform();
                        sceneEditor->SubmitGameObjectTransform(
                            *m_Context, m_GizmoSessionGameObjectId, m_GizmoStartTransform, endTransform);
                    }
                }
            }

            m_GizmoSessionGameObjectId = std::numeric_limits<uint64_t>::max();
        }

        m_GizmoWasUsing = usingNow;
    }



    void SceneEditingViewportClient::TrySelectAtMousePosition()

    {

        Vector2 mousePosition = InputSystem::GetMousePosition();

        mousePosition -= m_FrameState.ImageMin;



        Vector2 viewportSize = m_FrameState.ImageSize;

        if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)

        {

            return;

        }



        const Vector2 sceneBufferSize = GetSceneViewport().GetBufferSize();

        if (sceneBufferSize.x <= 0.0f || sceneBufferSize.y <= 0.0f)

        {

            return;

        }



        const float xRatio = sceneBufferSize.x / viewportSize.x;

        const float yRatio = sceneBufferSize.y / viewportSize.y;

        const Vector2 scaledMousePosition = mousePosition * Vector2(xRatio, yRatio);



        RenderCamera* viewportCamera = GetSceneViewport().GetCamera();

        if (!viewportCamera)

        {

            return;

        }



        const Geometry::Ray pickRay = viewportCamera->ScreenPointToRay(scaledMousePosition, sceneBufferSize);



        RenderScene* scene = GetSceneViewport().GetObservedScene();

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

            const bool intersected = worldBoundingBox.IntersectRay(pickRay, distance);

            if (intersected && distance < closestHitDistance)

            {

                closestHitDistance = distance;

                closestHitObject = staticMeshComponent->GetOwner();

            }

        }



        if (closestHitObject)

        {

            if (SceneEditor* sceneEditor = GetSceneEditor(m_Context))
            {
                sceneEditor->SelectGameObject(closestHitObject->GetID());
            }

        }

    }



    bool SceneEditingViewportClient::ApplyLookFromMouse()

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



    bool SceneEditingViewportClient::ApplyMovementFromCommands(const std::vector<ViewportInputCommand>& commands,

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



    void SceneEditingViewportClient::ApplyMoveSpeedFromScroll()

    {

        const Vector2 scrollDelta = InputSystem::GetMouseScrollDelta();

        if (Math::abs(scrollDelta.y) < 0.0001f)

        {

            return;

        }



        m_MoveSpeed += scrollDelta.y * m_MoveSpeedStep;

        m_MoveSpeed = std::clamp(m_MoveSpeed, m_MoveSpeedMin, m_MoveSpeedMax);

    }



    void SceneEditingViewportClient::SyncRenderTargetSize()
    {
        EditorViewportClient::SyncRenderTargetSize();
    }

}

