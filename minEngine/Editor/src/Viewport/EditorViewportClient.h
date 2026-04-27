#pragma once

#include "Core.h"
#include "Math/Math.h"

#include "Runtime/Function/Input/InputKeys.h"

#include <cstdint>
#include <limits>

namespace minEngine
{
    class Editor;
    class RenderCamera;
    class RenderScene;
    class PrimitiveSceneProxy;
    class GameObject;
    struct AABB;

    struct ViewportFrameState
    {
        bool Hovered = false;
        bool Focused = false;
        Vector2 ContentSize;
        Vector2 ImageMin;
        Vector2 ImageSize;
    };

    enum class ViewportInputTriggerType : uint8_t
    {
        Pressed,
        Released,
        Down
    };

    enum class ViewportInputCommandType : uint8_t
    {
        BeginNavigate,
        EndNavigate,
        MoveForward,
        MoveBackward,
        MoveLeft,
        MoveRight,
        MoveUp,
        MoveDown,
        SpeedBoost,
        AdjustMoveSpeed,
        FocusSelection,
        Cancel,
        Select
    };

    struct ViewportInputBinding
    {
        InputKey Key;
        ViewportInputTriggerType Trigger = ViewportInputTriggerType::Pressed;
        ViewportInputCommandType Command = ViewportInputCommandType::Cancel;
    };

    struct ViewportInputCommand
    {
        ViewportInputCommandType Type = ViewportInputCommandType::Cancel;
        ViewportInputTriggerType Trigger = ViewportInputTriggerType::Pressed;
    };

    struct ViewportPickQuery
    {
        const RenderCamera* Camera = nullptr;
        const RenderScene* Scene = nullptr;
        Vector2 MousePosition = Vector2(0.0f, 0.0f);
    };

    struct ViewportPickHitResult
    {
        GameObject* HitGameObject = nullptr;
        const PrimitiveSceneProxy* HitPrimitive = nullptr;
        float HitDistance = std::numeric_limits<float>::max();
        Vector3 HitPosition = Vector3(0.0f, 0.0f, 0.0f);
    };

    // Backend state holder for one viewport window.
    class EditorViewportClient
    {
    public:
        explicit EditorViewportClient(std::string debugName = "Viewport");

        void BeginFrame(float deltaTime);
        void UpdateFrameState(const ViewportFrameState& frameState);
        void EndFrame();
        void InputKeys();

        const ViewportFrameState& GetFrameState() const { return m_FrameState; }
        bool IsHovered() const { return m_FrameState.Hovered; }
        bool IsFocused() const { return m_FrameState.Focused; }
        bool IsNavigating() const { return m_IsNavigating; }
        float GetLastDeltaTime() const { return m_LastDeltaTime; }

        void SetInputBlockedByGizmo(bool blocked);
        bool IsInputBlockedByGizmo() const { return m_InputBlockedByGizmo; }

        const std::vector<ViewportInputCommand>& GetPendingInputCommands() const { return m_PendingInputCommands; }
        std::vector<ViewportInputCommand> ConsumePendingInputCommands();

    private:
        void RegisterDefaultInputBindings();
        void EvaluateInputBindings();
        bool IsBindingTriggered(const ViewportInputBinding& binding) const;
        void EmitInputCommand(ViewportInputCommandType commandType, ViewportInputTriggerType triggerType);
        void ExecuteInputCommands();

        void EnsureCameraStateInitialized(const RenderCamera& camera);
        void SyncStateFromRenderCamera(const RenderCamera& camera);
        void ApplyStateToRenderCamera(RenderCamera& camera);
        void SetNavigating(bool navigating);

        void TrySelectAtMousePosition();

        bool ApplyLookFromMouse();
        bool ApplyMovementFromCommands(const std::vector<ViewportInputCommand>& commands, bool speedBoostEnabled);
        void ApplyMoveSpeedFromScroll();
        void SyncRenderTargetSize();

    public:
        Editor* m_Editor = nullptr;

    private:
        std::string m_DebugName;
        ViewportFrameState m_FrameState;
        std::vector<ViewportInputBinding> m_InputBindings;
        std::vector<ViewportInputCommand> m_PendingInputCommands;
        bool m_DefaultInputBindingsRegistered = false;
        bool m_InputBlockedByGizmo = false;
        bool m_IsNavigating = false;

        bool m_CameraStateInitialized = false;
        bool m_HasLastMousePositionSample = false;
        Vector2 m_LastMousePosition = Vector2(0.0f, 0.0f);
        Vector3 m_CameraPosition = Vector3(0.0f, 0.0f, 0.0f);
        Vector3 m_CameraRotation = Vector3(0.0f, 0.0f, 0.0f);
        float m_MoveSpeed = 3.0f;
        float m_MoveSpeedMin = 0.5f;
        float m_MoveSpeedMax = 64.0f;
        float m_MoveSpeedStep = 1.0f;
        float m_BoostMultiplier = 4.0f;
        float m_MouseSensitivity = 0.08f;
        float m_MinPitch = -89.0f;
        float m_MaxPitch = 89.0f;
        float m_LastDeltaTime = 0.0f;

        uint32_t m_LastRequestedWidth = 0;
        uint32_t m_LastRequestedHeight = 0;
    };
}
