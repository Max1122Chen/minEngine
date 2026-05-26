#pragma once



#include "Viewport/EditorViewportClient.h"



#include <limits>

#include <vector>



namespace minEngine

{

    class RenderCamera;

    class RenderScene;

    class GameObject;
    struct Transform;

    class RHI;



    /** Scene editing viewport: fly camera, picking, gizmo consumption, owned scene RT. */

    class SceneEditingViewportClient : public EditorViewportClient

    {

    public:

        explicit SceneEditingViewportClient(std::string debugName = "Scene Editing Viewport");

        ~SceneEditingViewportClient() override;



        void BeginFrame(float deltaTime) override;

        void EndFrame() override;



        void InitializeSceneViewport(RHI* rhi, uint32_t width, uint32_t height);

        GizmoState& GetGizmoState() { return m_GizmoState; }

        bool IsNavigating() const { return m_IsNavigating; }



        void SetInputBlockedByGizmo(bool blocked);

        bool IsInputBlockedByGizmo() const { return m_InputBlockedByGizmo; }



        const std::vector<ViewportInputCommand>& GetPendingInputCommands() const { return m_PendingInputCommands; }

        std::vector<ViewportInputCommand> ConsumePendingInputCommands();



    protected:

        void SyncRenderTargetSize() override;



    private:

        void RegisterDefaultInputBindings();

        void EvaluateInputBindings();

        bool IsBindingTriggered(const ViewportInputBinding& binding) const;

        void EmitInputCommand(ViewportInputCommandType commandType, ViewportInputTriggerType triggerType);

        void InputKeys();

        void ExecuteInputCommands();

        void SyncObservedScene();



        void EnsureCameraStateInitialized(const RenderCamera& camera);

        void SyncStateFromRenderCamera(const RenderCamera& camera);

        void ApplyStateToRenderCamera(RenderCamera& camera);

        void SetNavigating(bool navigating);



        void ConsumeGizmoManipulation();
        void BeginGizmoCommandSessionIfNeeded();
        void EndGizmoCommandSessionIfNeeded();

        void TrySelectAtMousePosition();



        bool ApplyLookFromMouse();

        bool ApplyMovementFromCommands(const std::vector<ViewportInputCommand>& commands, bool speedBoostEnabled);

        void ApplyMoveSpeedFromScroll();

        GizmoState m_GizmoState;

        std::vector<ViewportInputBinding> m_InputBindings;

        std::vector<ViewportInputCommand> m_PendingInputCommands;

        bool m_DefaultInputBindingsRegistered = false;

        bool m_InputBlockedByGizmo = false;
        bool m_GizmoWasUsing = false;
        uint64_t m_GizmoSessionGameObjectId = std::numeric_limits<uint64_t>::max();
        Transform m_GizmoStartTransform;

        bool m_IsNavigating = false;



        bool m_CameraStateInitialized = false;

        bool m_HasLastMousePositionSample = false;

        Vector2 m_LastMousePosition = Vector2(0.0f, 0.0f);

        Vector3 m_CameraPosition = Vector3(0.0f, 0.0f, 0.0f);

        Vector3 m_CameraRotation = Vector3(0.0f, 0.0f, 0.0f);

        float m_MoveSpeed = 10.0f;

        float m_MoveSpeedMin = 0.5f;

        float m_MoveSpeedMax = 64.0f;

        float m_MoveSpeedStep = 1.0f;

        float m_BoostMultiplier = 4.0f;

        float m_MouseSensitivity = 0.08f;

        float m_MinPitch = -89.0f;

        float m_MaxPitch = 89.0f;

    };

}

