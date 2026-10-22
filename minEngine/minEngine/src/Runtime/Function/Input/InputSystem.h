#pragma once
#include "Core.h"
#include "InputKeys.h"
#include "InputKeyTypes.h"
#include "InputKeyState.h"
#include "InputAction.h"


namespace minEngine
{
    class Engine;
    class InputComponent;
    class InputMappingContext;

    struct ActiveInputMappingContext
    {
        InputMappingContext* Context;
        int Priority = 0;
    };

    /**
     * @brief 
     * A simple input system that handles input events, manages input mappings, and processes input actions.
     * Inspired by Unreal Engine's Enhanced Input System.
     * Implemented Feature List:
     * - Input Mapping Context ( IMC ). With InputKeyActionMapping.
     * - Input Actions ( IA ). With InputTrigger and InputModifier.
     */
    class InputSystem       
    {
    // TODO: To support multiple players later, we may need to have PlayerInputSystem or LocalInputSystem
    public:
        InputSystem() = default;
        ~InputSystem() = default;

        void Initialize();
        void Shutdown();

        static bool HasInstance() { return s_Instance != nullptr; }
        static InputSystem& Get();

        void Tick(float deltaTime);

        /**
         * whether gameplay IMC→IA should update this frame.
         * - HasBegunPlayWorld → true (PIE / host after DispatchBeginPlay), and when an Editor
         * viewport exists, also requires EditorGameplayCaptureActive.
         * - Editor shell (viewport registered) without begun world → false (Edit)
         * - No editor shell → true (standalone / Playground compat until host DispatchBeginPlay)
         */
        bool IsGameplayInputEnabled() const;

        /**
         * Editor sets false while PIE pointer is Released (still Playing).
         * Ignored when no Editor scene viewport is registered (standalone stays on).
         * Default true so BeginPlay without an explicit release still enables gameplay.
         */
        void SetEditorGameplayCaptureActive(bool active);
        bool IsEditorGameplayCaptureActive() const { return m_bEditorGameplayCaptureActive; }

        void AddInputComponent(InputComponent* component);
        void RemoveInputComponent(InputComponent* component);


        // Input Mapping Context Management. 
        // Maybe move it to something like "PlayerLocalInputSystem" later
        void AddInputMappingContext(InputMappingContext* context, int priority = 0);
        void RemoveInputMappingContext(InputMappingContext* context);

        InputActionInstance* FindInputActionInstance(InputAction* action);

        /**
         * True when the latest Tick found a cycle among chord trigger dependencies.
         * A dependency cycle has no evaluation order; the cycle is reported and the closing
         * dependency is skipped so the frame still completes instead of recursing forever.
         */
        bool HasChordCycleError() const { return m_bChordCycleDetected; }

        static bool KeyPressed(const InputKey& key) { return Get().GetKeyState(key)->action == InputKeyAction::Press; }
        static bool KeyDown(const InputKey& key) { return Get().GetKeyState(key)->action & (InputKeyAction::Press | InputKeyAction::Down); }
        static bool KeyReleased(const InputKey& key) { return Get().GetKeyState(key)->action == InputKeyAction::Release; }

        static bool CursorMoved() { return Get().GetKeyState(InputKeys::Mouse2D)->action != InputKeyAction::Idle; }
        static bool MouseScrolled() { return Get().GetKeyState(InputKeys::MouseScroll)->action == InputKeyAction::Down; }
        static bool MouseButtonPressed(const InputKey& key) { return KeyPressed(key); }
        static bool MouseButtonDown(const InputKey& key) { return KeyDown(key); }
        static bool MouseButtonReleased(const InputKey& key) { return KeyReleased(key); }

        static Vector2 GetMousePosition();
        /** Absolute Mouse2D key value (virtual while cursor-clip preserve is on; physical otherwise). */
        static Vector2 GetMouse2DAbsolute();
        static Vector2 GetMouseScrollDelta();

        /**
         * When true (PIE cursor clip): Mouse2D for IMC look tracks virtual absolute from Raw Input
         * so intent continues at ClipCursor edges; GetMousePosition stays physical for ScreenUI.
         */
        void SetPreserveMouseIntentBeyondClip(bool enabled);

    private:
        void OnKey(InputKey key, int scancode, InputKeyAction action, int mods);
        void OnMouseButton(InputKey key, InputKeyAction action, int mods);
        void OnCursorPos(double xPos, double yPos);
        void OnMouseScroll(double xOffset, double yOffset);
        void OnKeyOrMouseButton_Internal(InputKey key, int scancode, InputKeyAction action, int mods);
        InputKeyAction CalculateKeyAction(InputKeyAction prevAction, InputKeyAction newAction);
        InputKeyState* GetKeyState(const InputKey& key) { return &m_KeyStateMap[key]; }
        const InputKeyState* GetKeyState(const InputKey& key) const { return &m_KeyStateMap.at(key); }

        bool IsKeyHeld(const InputKey& key) const;
        void RefreshSyntheticModifier(
            const InputKey& synthetic,
            const InputKey& left,
            const InputKey& right);
        void RefreshSyntheticModifiersForSideKey(const InputKey& sideKey);
        void ApplySyntheticEdge(const InputKey& synthetic, bool anySideHeld);
        void PollGamepadDevices();
        void WriteGamepadButton(const InputKey& key, bool bPressed);
        void WriteGamepadAxis(const InputKey& key, float value);
        void ClearGamepadStates();

        InputActionValue ApplyModifiers(const std::vector<std::shared_ptr<InputModifier>>& modifiers, const InputActionValue& rawValue, float deltaTime);
        InputTriggerEvent GetTriggerStateChangeEvent(InputTriggerState lastState, InputTriggerState newState);

        /** Step 1 of the frame: drop last frame's events and value, keep the previous state. */
        void StartFrame();
        /** Step 2: sum this frame's mapping values and advance every mapping trigger. */
        void EvaluateMappingContributions(float deltaTime);
        /** Steps 3-5: action value, then triggers, then the event implied by the state change. */
        void EvaluateActionInstances(const std::vector<InputAction*>& evaluationOrder, float deltaTime);
        /** Chord dependencies first, so a chord trigger reads a state computed in the same frame. */
        std::vector<InputAction*> BuildActionEvaluationOrder();
        void VisitActionDependencies(InputAction* action, std::unordered_map<InputAction*, int>& visitState, std::vector<InputAction*>& evaluationOrder);
        void CollectChordDependencies(InputAction* action, std::vector<InputAction*>& outDependencies);
        /** Hands each chord trigger the state its dependency reached, before the trigger evaluates. */
        void PushChordDependencyState(const std::vector<std::shared_ptr<InputTrigger>>& triggers);
        void ReportChordCycle(const InputAction* action);
        /** Step 6: hand the frame's events to the registered input components. */
        void NotifyInputComponents(const std::vector<InputAction*>& evaluationOrder);
        /** Per-frame device edge cleanup (e.g. MouseScroll sticky Down). Runs even when gameplay IMC is gated off. */
        void EndFrameDeviceEdgeCleanup();
        void WriteMouse2DState(const Vector2& position);
        void ApplyRelativeMouseIntentFromWindow();

    private:
        friend class Engine;
        friend class Testing::TestAccess<InputSystem>;

        static void SetInstance(InputSystem* instance);
        static InputSystem* s_Instance;

        InputKeys m_InputKeys;
        std::unordered_map<InputKey, InputKeyState, InputKey::Hash> m_KeyStateMap;

        std::shared_ptr<InputMappingContext> m_DefaultContext;
        std::vector<ActiveInputMappingContext> m_ActiveContexts;

        std::unordered_map<InputAction*, InputActionInstance> m_ActionInstances;
        bool m_bChordCycleDetected = false;

        // Registered Input Components
        std::vector<InputComponent*> m_InputComponents;

        Vector2 m_PhysicalMousePos = Vector2(0.0f, 0.0f);
        Vector2 m_VirtualMousePos = Vector2(0.0f, 0.0f);
        bool m_bPreserveMouseIntentBeyondClip = false;
        bool m_bEditorGameplayCaptureActive = true;
        bool m_bUpdatingSynthetic = false;
        bool m_bGamepadConnected = false;
    };
}