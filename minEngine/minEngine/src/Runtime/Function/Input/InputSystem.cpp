#include "InputSystem.h"

#include "InputKeys.h"
#include "InputKeyState.h"

#include "InputMappingContext.h"

#include "InputModifiers.h"
#include "Framework/Components/InputComponent.h"

#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RenderCamera.h"

#include "glfw/glfw3.h"
#include "Runtime/Core/Math/Math.h"
#include "glm/glm.hpp"


namespace minEngine
{
    InputSystem* InputSystem::s_Instance = nullptr;

    void InputSystem::SetInstance(InputSystem* instance)
    {
        s_Instance = instance;
    }

    InputSystem& InputSystem::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "InputSystem is not initialized");
        return *s_Instance;
    }

    bool InputSystem::IsGameplayInputEnabled() const
    {
        if (!SceneManager::HasInstance())
        {
            return true;
        }

        const SceneManager& sceneManager = SceneManager::Get();
        const bool hasEditorShell = sceneManager.GetEditorSceneViewport() != nullptr;

        if (sceneManager.HasBegunPlayWorld())
        {
            // while Editor shell is up, Released PIE must gate IMC even though BeginPlay is true.
            if (hasEditorShell)
            {
                return m_bEditorGameplayCaptureActive;
            }
            return true;
        }

        // Editor document Edit: viewport registered, no simulating world.
        if (hasEditorShell)
        {
            return false;
        }

        // Standalone / tests without editor shell — keep prior always-on until host DispatchBeginPlay.
        return true;
    }

    void InputSystem::SetEditorGameplayCaptureActive(bool active)
    {
        m_bEditorGameplayCaptureActive = active;
    }

    void InputSystem::StartFrame()
    {
        // Every action instance is rebuilt from this frame's mappings, so a value never survives
        // a frame in which no mapping is active. LastState is deliberately kept: it is the only
        // previous-state source the event derivation has.
        m_bChordCycleDetected = false;
        for (auto& [action, instance] : m_ActionInstances)
        {
            (void)action;
            instance.Value.Reset();
            instance.TriggerEvent = InputTriggerEvent::None;
        }
    }

    void InputSystem::EndFrameDeviceEdgeCleanup()
    {
        // Scroll is edge-triggered; clear after the frame so KeyDown(MouseScroll) is one-shot.
        InputKeyState* mouseScrollState = GetKeyState(InputKeys::MouseScroll);
        if (mouseScrollState->action == InputKeyAction::Down)
        {
            mouseScrollState->action = InputKeyAction::Idle;
            mouseScrollState->RawValue = Vector3(0.0f, 0.0f, 0.0f);
        }
    }

    void InputSystem::WriteMouse2DState(const Vector2& position)
    {
        InputKeyState* keyState = GetKeyState(InputKeys::Mouse2D);
        keyState->action = (Math::abs(position.x) > 0.1f || Math::abs(position.y) > 0.1f)
            ? InputKeyAction::Down
            : InputKeyAction::Idle;
        keyState->RawValue = Vector3(position.x, position.y, 0.0f);
    }

    void InputSystem::SetPreserveMouseIntentBeyondClip(bool enabled)
    {
        if (m_bPreserveMouseIntentBeyondClip == enabled)
        {
            return;
        }

        m_bPreserveMouseIntentBeyondClip = enabled;
        if (enabled)
        {
            m_VirtualMousePos = m_PhysicalMousePos;
            WriteMouse2DState(m_VirtualMousePos);
        }
        else
        {
            m_VirtualMousePos = m_PhysicalMousePos;
            WriteMouse2DState(m_PhysicalMousePos);
        }
    }

    void InputSystem::ApplyRelativeMouseIntentFromWindow()
    {
        if (!m_bPreserveMouseIntentBeyondClip || !WindowSystem::HasInstance())
        {
            return;
        }

        const Vector2 relativeDelta = WindowSystem::Get().ConsumeRelativeMouseDelta();
        if (Math::abs(relativeDelta.x) < 0.0001f && Math::abs(relativeDelta.y) < 0.0001f)
        {
            return;
        }

        m_VirtualMousePos.x += relativeDelta.x;
        m_VirtualMousePos.y += relativeDelta.y;
        WriteMouse2DState(m_VirtualMousePos);
    }

    void InputSystem::Initialize()
    {
        if (WindowSystem::HasInstance())
        {
            WindowSystem* windowSystem = &WindowSystem::Get();

            // TODO: maybe we will wrap these logic into a private function later
            windowSystem->RegisterOnKeyCallback([this](InputKey key, int scancode, InputKeyAction action, int mods)
            {
                this->OnKey(key, scancode, action, mods);
            });

            windowSystem->RegisterOnCursorPosCallback([this](double xPos, double yPos)
            {
                this->OnCursorPos(xPos, yPos);
            });

            windowSystem->RegisterOnMouseScrollCallback([this](double xOffset, double yOffset)
            {
                this->OnMouseScroll(xOffset, yOffset);
            });
        }
    
        m_InputKeys.Initialize();
        for(const auto& key : m_InputKeys.GetAllKeys())
        {
            m_KeyStateMap.emplace(key, InputKeyState{});
        }

        ME_LOG(LogCore, Info, "InputSystem Initialized"); 
    }

    void InputSystem::Shutdown()
    {
        m_InputComponents.clear();
        m_ActiveContexts.clear();
        m_ActionInstances.clear();
        m_bChordCycleDetected = false;
        m_KeyStateMap.clear();

        m_DefaultContext.reset();

        ME_LOG(LogCore, Info, "InputSystem Shutdown");
    }

    void InputSystem::Tick(float deltaTime)
    {
        StartFrame();

        // Apply Raw Input deltas before IMC so look-at-edge still advances Mouse2D while clipped.
        ApplyRelativeMouseIntentFromWindow();

        // Device state (gamepad) stays fresh even when gameplay IMC is gated off.
        PollGamepadDevices();

        // A gated frame is simply a frame in which no mapping contributes: the triggers still run,
        // so an action that was held across the gate releases on the first frame it is allowed to.
        if (IsGameplayInputEnabled())
        {
            EvaluateMappingContributions(deltaTime);
        }

        const std::vector<InputAction*> evaluationOrder = BuildActionEvaluationOrder();
        EvaluateActionInstances(evaluationOrder, deltaTime);
        NotifyInputComponents(evaluationOrder);

        EndFrameDeviceEdgeCleanup();
    }

    void InputSystem::EvaluateMappingContributions(float deltaTime)
    {
        for (auto& activeContext : m_ActiveContexts)
        {
            if (!activeContext.Context)
            {
                continue;
            }

            for (const auto& mapping : activeContext.Context->GetMappings())
            {
                InputAction* action = mapping.Action;
                if (!action)
                {
                    continue;
                }

                InputActionInstance* instance = FindInputActionInstance(action);
                if (!instance)
                {
                    continue;
                }

                const InputKeyState* keyState = GetKeyState(mapping.Key);
                const bool bMappingActive =
                    (keyState->action & (InputKeyAction::Press | InputKeyAction::Down)) != 0;

                const InputActionValue rawValue =
                    bMappingActive ? InputActionValue(keyState->RawValue) : InputActionValue{};
                const InputActionValue modifiedValue =
                    ApplyModifiers(mapping.GetModifiers(), rawValue, deltaTime);

                // Mapping triggers advance on every frame, active or not, because each trigger keeps
                // its own activation memory: a Pressed trigger that is only fed while its key is down
                // never observes the release that would re-arm it. Their state is not folded into the
                // action below - a mapping without triggers reports the level state, and merging that
                // into an edge-triggered action would re-fire the action on every held frame.
                PushChordDependencyState(mapping.GetTriggers());
                InputTriggerStateTracker mappingTriggerState;
                mappingTriggerState.EvaluateTriggerState(mapping.GetTriggers(), modifiedValue, deltaTime);

                if (bMappingActive)
                {
                    // A mapping that is not active contributes zero, not the key's last value.
                    instance->Value += modifiedValue;
                }
            }
        }
    }

    std::vector<InputAction*> InputSystem::BuildActionEvaluationOrder()
    {
        std::vector<InputAction*> evaluationOrder;
        evaluationOrder.reserve(m_ActionInstances.size());

        // 0 = not visited, 1 = on the current dependency chain, 2 = already ordered.
        std::unordered_map<InputAction*, int> visitState;
        for (auto& [action, instance] : m_ActionInstances)
        {
            (void)instance;
            VisitActionDependencies(action, visitState, evaluationOrder);
        }
        return evaluationOrder;
    }

    void InputSystem::VisitActionDependencies(
        InputAction* action,
        std::unordered_map<InputAction*, int>& visitState,
        std::vector<InputAction*>& evaluationOrder)
    {
        int& state = visitState[action];
        if (state == 2)
        {
            return;
        }

        if (state == 1)
        {
            // A chord chain that leads back into an action still on the chain has no evaluation
            // order. Report it and drop this dependency instead of guessing or truncating silently.
            ReportChordCycle(action);
            return;
        }

        state = 1;
        std::vector<InputAction*> dependencies;
        CollectChordDependencies(action, dependencies);
        for (InputAction* dependency : dependencies)
        {
            if (dependency != nullptr && FindInputActionInstance(dependency) != nullptr)
            {
                VisitActionDependencies(dependency, visitState, evaluationOrder);
            }
        }
        state = 2;
        evaluationOrder.push_back(action);
    }

    void InputSystem::CollectChordDependencies(InputAction* action, std::vector<InputAction*>& outDependencies)
    {
        for (const auto& trigger : action->GetTriggers())
        {
            if (trigger && trigger->GetChordAction() != nullptr)
            {
                outDependencies.push_back(trigger->GetChordAction());
            }
        }

        for (const auto& activeContext : m_ActiveContexts)
        {
            if (!activeContext.Context)
            {
                continue;
            }

            for (const auto& mapping : activeContext.Context->GetMappings())
            {
                if (mapping.Action != action)
                {
                    continue;
                }

                for (const auto& trigger : mapping.GetTriggers())
                {
                    if (trigger && trigger->GetChordAction() != nullptr)
                    {
                        outDependencies.push_back(trigger->GetChordAction());
                    }
                }
            }
        }
    }

    void InputSystem::PushChordDependencyState(const std::vector<std::shared_ptr<InputTrigger>>& triggers)
    {
        for (const auto& trigger : triggers)
        {
            if (!trigger)
            {
                continue;
            }

            InputAction* dependency = trigger->GetChordAction();
            if (dependency == nullptr)
            {
                continue;
            }

            const InputActionInstance* dependencyInstance = FindInputActionInstance(dependency);
            trigger->SetChordDependencyState(
                dependencyInstance != nullptr ? dependencyInstance->LastState : InputTriggerState::None);
        }
    }

    void InputSystem::ReportChordCycle(const InputAction* action)
    {
        if (m_bChordCycleDetected)
        {
            return;
        }

        m_bChordCycleDetected = true;
        ME_LOG(LogCore, Error,
            "Input chord cycle at action '{0}': chord dependencies must form an acyclic graph. "
            "The closing dependency is skipped so this frame still evaluates.",
            action != nullptr ? action->GetName() : std::string("<null>"));
    }

    void InputSystem::EvaluateActionInstances(const std::vector<InputAction*>& evaluationOrder, float deltaTime)
    {
        for (InputAction* action : evaluationOrder)
        {
            InputActionInstance* instance = FindInputActionInstance(action);
            if (!instance)
            {
                continue;
            }

            // Action modifiers see the summed mapping value; the trigger and the consumer see the
            // same modified value, so both agree on which frame the input crossed its threshold.
            InputActionValue modifiedValue = ApplyModifiers(action->GetModifiers(), instance->Value, deltaTime);
            instance->Value = modifiedValue;

            const InputTriggerState prevState = instance->LastState;

            PushChordDependencyState(action->GetTriggers());
            instance->TriggerStateTracker.Reset();
            instance->TriggerStateTracker.SetNoTriggerState(
                modifiedValue.IsNonZero() ? InputTriggerState::Triggered : InputTriggerState::None);
            InputTriggerState newState =
                instance->TriggerStateTracker.EvaluateTriggerState(action->GetTriggers(), modifiedValue, deltaTime);

            if (newState == InputTriggerState::None && !instance->TriggerStateTracker.IsBlocked()
                && modifiedValue.IsNonZero())
            {
                // An edge trigger reports None as soon as its edge is behind it, even while the input
                // is still held. Reporting None here would end the press mid-hold and turn the next
                // release into a completion of nothing; Ongoing keeps the press open. A blocker still
                // wins, and a trigger that holds its own state (Down, Released) is untouched.
                newState = InputTriggerState::Ongoing;
            }

            instance->TriggerEvent = GetTriggerStateChangeEvent(prevState, newState);
            instance->LastState = newState;
        }
    }

    void InputSystem::NotifyInputComponents(const std::vector<InputAction*>& evaluationOrder)
    {
        for (InputAction* action : evaluationOrder)
        {
            InputActionInstance* instance = FindInputActionInstance(action);
            if (!instance || instance->TriggerEvent == InputTriggerEvent::None)
            {
                continue;
            }

            for (auto* inputComponent : m_InputComponents)
            {
                inputComponent->ProcessInputAction(action, instance->TriggerEvent, instance->Value);
            }
        }
    }


    void InputSystem::AddInputComponent(InputComponent *component)
    {
        if (!component)
        {
            return;
        }

        if (std::find(m_InputComponents.begin(), m_InputComponents.end(), component) != m_InputComponents.end())
        {
            return;
        }

        m_InputComponents.push_back(component);
    }

    void InputSystem::RemoveInputComponent(InputComponent *component)
    {
        m_InputComponents.erase(
            std::remove(m_InputComponents.begin(), m_InputComponents.end(), component),
            m_InputComponents.end());
    }

    void InputSystem::AddInputMappingContext(InputMappingContext *context, int priority)
    {
        if (!context)
        {
            return;
        }

        m_ActiveContexts.push_back({context, priority});
        std::sort(m_ActiveContexts.begin(), m_ActiveContexts.end(), 
        [](const ActiveInputMappingContext& a, const ActiveInputMappingContext& b)
        {
            return a.Priority > b.Priority; // Higher priority first
        });

        // setup action instances
        for(const auto& mapping : context->GetMappings())
        {
            // create action instance if not exist
            if(m_ActionInstances.find(mapping.Action) == m_ActionInstances.end())
                m_ActionInstances.emplace(mapping.Action, InputActionInstance{mapping.Action});
        }
    }

    void InputSystem::RemoveInputMappingContext(InputMappingContext *context)
    {
        if (!context)
        {
            return;
        }

        m_ActiveContexts.erase(
            std::remove_if(m_ActiveContexts.begin(), m_ActiveContexts.end(),
                [context](const ActiveInputMappingContext& activeContext)
                {
                    return activeContext.Context == context;
                }),
            m_ActiveContexts.end());

        // Rebuild action instances from remaining active contexts.
        m_ActionInstances.clear();
        for (const auto& activeContext : m_ActiveContexts)
        {
            if (!activeContext.Context)
            {
                continue;
            }

            for (const auto& mapping : activeContext.Context->GetMappings())
            {
                if (mapping.Action && m_ActionInstances.find(mapping.Action) == m_ActionInstances.end())
                {
                    m_ActionInstances.emplace(mapping.Action, InputActionInstance{mapping.Action});
                }
            }
        }
    }

    InputActionInstance *InputSystem::FindInputActionInstance(InputAction *action)
    {
        auto it = m_ActionInstances.find(action);
        if(it != m_ActionInstances.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    Vector2 InputSystem::GetMousePosition()
    {
        return Get().m_PhysicalMousePos;
    }

    Vector2 InputSystem::GetMouse2DAbsolute()
    {
        auto it = Get().m_KeyStateMap.find(InputKeys::Mouse2D);
        if (it != Get().m_KeyStateMap.end())
        {
            return Vector2(it->second.RawValue.x, it->second.RawValue.y);
        }
        return Get().m_PhysicalMousePos;
    }

    Vector2 InputSystem::GetMouseScrollDelta()
    {
        auto it = Get().m_KeyStateMap.find(InputKeys::MouseScroll);
        if(it != Get().m_KeyStateMap.end())
        {
            return Vector2(it->second.RawValue.x, it->second.RawValue.y);
        }
        return Vector2(0.0f, 0.0f);
    }

    void InputSystem::OnKey(InputKey key, int scancode, InputKeyAction action, int mods)
    {
        OnKeyOrMouseButton_Internal(key, scancode, action, mods);
    }

    void InputSystem::OnMouseButton(InputKey key, InputKeyAction action, int mods)
    {
        OnKeyOrMouseButton_Internal(key, 0, action, mods);
    }

    void InputSystem::OnCursorPos(double xPos, double yPos)
    {
        m_PhysicalMousePos = Vector2(static_cast<float>(xPos), static_cast<float>(yPos));

        if (!m_bPreserveMouseIntentBeyondClip)
        {
            m_VirtualMousePos = m_PhysicalMousePos;
            WriteMouse2DState(m_PhysicalMousePos);
        }
        // While preserve is on: Mouse2D advances from Raw Input only (see ApplyRelativeMouseIntentFromWindow).
    }

    void InputSystem::OnMouseScroll(double xOffset, double yOffset)
    {
        InputKeyState* scrollState = GetKeyState(InputKeys::MouseScroll);

        scrollState->action = InputKeyAction::Idle;
        scrollState->RawValue = Vector3(0.0f, 0.0f, 0.0f);

        scrollState->action = InputKeyAction::Down;
        scrollState->RawValue = Vector3(static_cast<float>(xOffset), static_cast<float>(yOffset), 0.0f);
    }

    bool InputSystem::IsKeyHeld(const InputKey& key) const
    {
        const auto it = m_KeyStateMap.find(key);
        if (it == m_KeyStateMap.end())
        {
            return false;
        }
        return (it->second.action & (InputKeyAction::Press | InputKeyAction::Down)) != 0;
    }

    void InputSystem::ApplySyntheticEdge(const InputKey& synthetic, bool anySideHeld)
    {
        auto it = m_KeyStateMap.find(synthetic);
        if (it == m_KeyStateMap.end())
        {
            return;
        }

        InputKeyState& syn = it->second;
        const bool wasHeld = (syn.action & (InputKeyAction::Press | InputKeyAction::Down)) != 0;

        if (anySideHeld && !wasHeld)
        {
            syn.action = CalculateKeyAction(syn.action, InputKeyAction::Press);
        }
        else if (!anySideHeld && wasHeld)
        {
            syn.action = CalculateKeyAction(syn.action, InputKeyAction::Release);
        }
        else if (anySideHeld && wasHeld && syn.action == InputKeyAction::Press)
        {
            // Promote Press→Down once either side reports sustained Down.
            syn.action = CalculateKeyAction(syn.action, InputKeyAction::Down);
        }

        syn.RawValue = (syn.action & (InputKeyAction::Press | InputKeyAction::Down)) != 0
            ? Vector3(1.0f, 0.0f, 0.0f)
            : Vector3(0.0f, 0.0f, 0.0f);
    }

    void InputSystem::RefreshSyntheticModifier(
        const InputKey& synthetic,
        const InputKey& left,
        const InputKey& right)
    {
        ApplySyntheticEdge(synthetic, IsKeyHeld(left) || IsKeyHeld(right));
    }

    void InputSystem::RefreshSyntheticModifiersForSideKey(const InputKey& sideKey)
    {
        if (sideKey == InputKeys::Key_LeftShift || sideKey == InputKeys::Key_RightShift)
        {
            RefreshSyntheticModifier(InputKeys::Key_Shift, InputKeys::Key_LeftShift, InputKeys::Key_RightShift);
        }
        else if (sideKey == InputKeys::Key_LeftControl || sideKey == InputKeys::Key_RightControl)
        {
            RefreshSyntheticModifier(InputKeys::Key_Control, InputKeys::Key_LeftControl, InputKeys::Key_RightControl);
        }
        else if (sideKey == InputKeys::Key_LeftAlt || sideKey == InputKeys::Key_RightAlt)
        {
            RefreshSyntheticModifier(InputKeys::Key_Alt, InputKeys::Key_LeftAlt, InputKeys::Key_RightAlt);
        }
        else if (sideKey == InputKeys::Key_LeftSuper || sideKey == InputKeys::Key_RightSuper)
        {
            RefreshSyntheticModifier(InputKeys::Key_Super, InputKeys::Key_LeftSuper, InputKeys::Key_RightSuper);
        }
    }

    void InputSystem::WriteGamepadButton(const InputKey& key, bool bPressed)
    {
        auto it = m_KeyStateMap.find(key);
        if (it == m_KeyStateMap.end())
        {
            return;
        }

        InputKeyState& state = it->second;
        const bool wasHeld = (state.action & (InputKeyAction::Press | InputKeyAction::Down)) != 0;
        if (bPressed && !wasHeld)
        {
            state.action = CalculateKeyAction(state.action, InputKeyAction::Press);
        }
        else if (bPressed && wasHeld)
        {
            state.action = CalculateKeyAction(state.action, InputKeyAction::Down);
        }
        else if (!bPressed && wasHeld)
        {
            state.action = CalculateKeyAction(state.action, InputKeyAction::Release);
        }
        else if (!bPressed && state.action == InputKeyAction::Release)
        {
            state.action = InputKeyAction::Idle;
        }

        state.RawValue = bPressed ? Vector3(1.0f, 0.0f, 0.0f) : Vector3(0.0f, 0.0f, 0.0f);
    }

    void InputSystem::WriteGamepadAxis(const InputKey& key, float value)
    {
        auto it = m_KeyStateMap.find(key);
        if (it == m_KeyStateMap.end())
        {
            return;
        }

        InputKeyState& state = it->second;
        const bool bActive = Math::abs(value) > 0.0001f;
        state.action = bActive ? InputKeyAction::Down : InputKeyAction::Idle;
        state.RawValue = Vector3(value, 0.0f, 0.0f);
    }

    void InputSystem::ClearGamepadStates()
    {
        WriteGamepadButton(InputKeys::Gamepad_A, false);
        WriteGamepadButton(InputKeys::Gamepad_B, false);
        WriteGamepadButton(InputKeys::Gamepad_X, false);
        WriteGamepadButton(InputKeys::Gamepad_Y, false);
        WriteGamepadButton(InputKeys::Gamepad_LeftBumper, false);
        WriteGamepadButton(InputKeys::Gamepad_RightBumper, false);
        WriteGamepadButton(InputKeys::Gamepad_Back, false);
        WriteGamepadButton(InputKeys::Gamepad_Start, false);
        WriteGamepadButton(InputKeys::Gamepad_Guide, false);
        WriteGamepadButton(InputKeys::Gamepad_LeftThumb, false);
        WriteGamepadButton(InputKeys::Gamepad_RightThumb, false);
        WriteGamepadButton(InputKeys::Gamepad_DPad_Up, false);
        WriteGamepadButton(InputKeys::Gamepad_DPad_Right, false);
        WriteGamepadButton(InputKeys::Gamepad_DPad_Down, false);
        WriteGamepadButton(InputKeys::Gamepad_DPad_Left, false);
        WriteGamepadAxis(InputKeys::Gamepad_LeftX, 0.0f);
        WriteGamepadAxis(InputKeys::Gamepad_LeftY, 0.0f);
        WriteGamepadAxis(InputKeys::Gamepad_RightX, 0.0f);
        WriteGamepadAxis(InputKeys::Gamepad_RightY, 0.0f);
        WriteGamepadAxis(InputKeys::Gamepad_LeftTrigger, 0.0f);
        WriteGamepadAxis(InputKeys::Gamepad_RightTrigger, 0.0f);
    }

    void InputSystem::PollGamepadDevices()
    {
        int foundJoystick = -1;
        for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid)
        {
            if (glfwJoystickIsGamepad(jid) == GLFW_TRUE)
            {
                foundJoystick = jid;
                break;
            }
        }

        if (foundJoystick < 0)
        {
            if (m_bGamepadConnected)
            {
                ClearGamepadStates();
                m_bGamepadConnected = false;
            }
            return;
        }

        GLFWgamepadstate pad{};
        if (glfwGetGamepadState(foundJoystick, &pad) != GLFW_TRUE)
        {
            if (m_bGamepadConnected)
            {
                ClearGamepadStates();
                m_bGamepadConnected = false;
            }
            return;
        }

        m_bGamepadConnected = true;

        auto buttonDown = [&pad](int button) -> bool {
            return pad.buttons[button] == GLFW_PRESS;
        };

        WriteGamepadButton(InputKeys::Gamepad_A, buttonDown(GLFW_GAMEPAD_BUTTON_A));
        WriteGamepadButton(InputKeys::Gamepad_B, buttonDown(GLFW_GAMEPAD_BUTTON_B));
        WriteGamepadButton(InputKeys::Gamepad_X, buttonDown(GLFW_GAMEPAD_BUTTON_X));
        WriteGamepadButton(InputKeys::Gamepad_Y, buttonDown(GLFW_GAMEPAD_BUTTON_Y));
        WriteGamepadButton(InputKeys::Gamepad_LeftBumper, buttonDown(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER));
        WriteGamepadButton(InputKeys::Gamepad_RightBumper, buttonDown(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER));
        WriteGamepadButton(InputKeys::Gamepad_Back, buttonDown(GLFW_GAMEPAD_BUTTON_BACK));
        WriteGamepadButton(InputKeys::Gamepad_Start, buttonDown(GLFW_GAMEPAD_BUTTON_START));
        WriteGamepadButton(InputKeys::Gamepad_Guide, buttonDown(GLFW_GAMEPAD_BUTTON_GUIDE));
        WriteGamepadButton(InputKeys::Gamepad_LeftThumb, buttonDown(GLFW_GAMEPAD_BUTTON_LEFT_THUMB));
        WriteGamepadButton(InputKeys::Gamepad_RightThumb, buttonDown(GLFW_GAMEPAD_BUTTON_RIGHT_THUMB));
        WriteGamepadButton(InputKeys::Gamepad_DPad_Up, buttonDown(GLFW_GAMEPAD_BUTTON_DPAD_UP));
        WriteGamepadButton(InputKeys::Gamepad_DPad_Right, buttonDown(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT));
        WriteGamepadButton(InputKeys::Gamepad_DPad_Down, buttonDown(GLFW_GAMEPAD_BUTTON_DPAD_DOWN));
        WriteGamepadButton(InputKeys::Gamepad_DPad_Left, buttonDown(GLFW_GAMEPAD_BUTTON_DPAD_LEFT));

        WriteGamepadAxis(InputKeys::Gamepad_LeftX, pad.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
        WriteGamepadAxis(InputKeys::Gamepad_LeftY, pad.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
        WriteGamepadAxis(InputKeys::Gamepad_RightX, pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);
        WriteGamepadAxis(InputKeys::Gamepad_RightY, pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
        WriteGamepadAxis(InputKeys::Gamepad_LeftTrigger, pad.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]);
        WriteGamepadAxis(InputKeys::Gamepad_RightTrigger, pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]);
    }

    void InputSystem::OnKeyOrMouseButton_Internal(InputKey key, int scancode, InputKeyAction action, int mods)
    {
        (void)scancode;
        (void)mods;

        auto it = m_KeyStateMap.find(key);
        if (it == m_KeyStateMap.end())
        {
            return;
        }

        InputKeyState& keyState = it->second;
        const InputKeyAction prevAction = keyState.action;
        keyState.action = CalculateKeyAction(prevAction, action);
        keyState.RawValue = (keyState.action & (InputKeyAction::Press | InputKeyAction::Down)) != 0
            ? Vector3(1.0f, 0.0f, 0.0f)
            : Vector3(0.0f, 0.0f, 0.0f);

        if (!m_bUpdatingSynthetic)
        {
            m_bUpdatingSynthetic = true;
            RefreshSyntheticModifiersForSideKey(key);
            m_bUpdatingSynthetic = false;
        }
    }

    InputKeyAction InputSystem::CalculateKeyAction(InputKeyAction prevAction, InputKeyAction newAction)
    {
        switch(prevAction)
        {
            case InputKeyAction::Idle:
                switch(newAction)
                {
                    case InputKeyAction::Press:     return InputKeyAction::Press;
                    case InputKeyAction::Down:      return InputKeyAction::Press; // Treat as Press if we receive Down event while Idle, which can happen for mouse scroll input
                    default:                        return InputKeyAction::Idle;
                }
            case InputKeyAction::Press:
                switch(newAction)
                {
                    case InputKeyAction::Release:   return InputKeyAction::Release;
                    case InputKeyAction::Down:      return InputKeyAction::Down;
                    default:                        return InputKeyAction::Press;
                }
            case InputKeyAction::Down:
                switch(newAction)
                {
                    case InputKeyAction::Release:   return InputKeyAction::Release;
                    case InputKeyAction::Down:      return InputKeyAction::Down;
                    default:                        return InputKeyAction::Down;
                }
            case InputKeyAction::Release:
                switch(newAction)
                {
                    case InputKeyAction::Press:     return InputKeyAction::Press;
                    case InputKeyAction::Down:      return InputKeyAction::Press; // Treat as Press if we receive Down event while Release, which can happen for mouse scroll input
                    default:                        return InputKeyAction::Idle;
                }
            default:
                return InputKeyAction::Idle;
        }
    }

    InputActionValue InputSystem::ApplyModifiers(const std::vector<std::shared_ptr<InputModifier>> &modifiers, const InputActionValue &rawValue, float deltaTime)
    {
        InputActionValue modifiedValue = rawValue;
        for(auto modifier : modifiers)
        {
            modifiedValue = modifier->ModifyRaw(modifiedValue, deltaTime);
        }
        return modifiedValue;
    }

    InputTriggerEvent InputSystem::GetTriggerStateChangeEvent(InputTriggerState lastState, InputTriggerState newState)
    {
        switch(lastState)
        {
            case InputTriggerState::None:
                if(newState == InputTriggerState::Ongoing)
                {
                    return InputTriggerEvent::Started;
                }
                else if(newState == InputTriggerState::Triggered)
                {
                    return InputTriggerEvent::Triggered;
                }
                break;
            case InputTriggerState::Ongoing:
                if(newState == InputTriggerState::Triggered)
                {
                    return InputTriggerEvent::Triggered;
                }
                else if(newState == InputTriggerState::None)
                {
                    return InputTriggerEvent::Canceled;
                }
                break;
            case InputTriggerState::Triggered:
                if(newState == InputTriggerState::Triggered)
                {
                    return InputTriggerEvent::Triggered;
                }
                else if(newState == InputTriggerState::Ongoing)
                {
                    return InputTriggerEvent::Ongoing;
                }
                else if(newState == InputTriggerState::None)
                {
                    return InputTriggerEvent::Completed;
                }
                break;
        }

        return InputTriggerEvent::None;
    }
}