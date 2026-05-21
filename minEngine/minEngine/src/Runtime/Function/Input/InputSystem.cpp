#include "InputSystem.h"

#include "InputKeys.h"
#include "InputKeyState.h"

#include "InputMappingContext.h"

#include "InputModifiers.h"
#include "Framework/Components/InputComponent.h"

#include "Runtime/Function/Render/WindowSystem.h"
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

    void InputSystem::Initialize()
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
    
        m_InputKeys.Initialize();
        for(const auto& key : m_InputKeys.GetAllKeys())
        {
            m_KeyStateMap.emplace(key, InputKeyState{});
        }

        ME_CORE_INFO("InputSystem Initialized"); 
    }

    void InputSystem::Shutdown()
    {
        m_InputComponents.clear();
        m_ActiveContexts.clear();
        m_ActionInstances.clear();
        m_ActionsWithEventThisTick.clear();
        m_KeyStateMap.clear();

        m_DefaultContext.reset();

        ME_CORE_INFO("InputSystem Shutdown");
    }

    void InputSystem::Tick(float deltaTime)
    {
        // Clear actions with events last frame
        m_ActionsWithEventThisTick.clear();

        // bool bConsumed = false; TODO: maybe we will need it later

        for(auto& activeContext : m_ActiveContexts)
        {
            if (!activeContext.Context)
            {
                continue;
            }

            for(const auto& mapping : activeContext.Context->GetMappings())
            {
                InputAction* action = mapping.Action;

                if(!action)
                {
                    continue;
                }

                InputTriggerStateTracker triggerStateTracker;

                InputActionInstance* instance = FindInputActionInstance(action);
                if (!instance)
                {
                    continue;
                }

                // Reset if we cannot find the action instance, which means its value is not updated this frame
                bool bShouldResetAction = !(m_ActionsWithEventThisTick.end() != 
                    std::find(m_ActionsWithEventThisTick.begin(), m_ActionsWithEventThisTick.end(), action));

                const InputKey& key = mapping.Key;
                if(GetKeyState(key)->action & (InputKeyAction::Press | InputKeyAction::Down))
                {
                    if(bShouldResetAction)
                    {
                        instance->Value.Reset();
                        m_ActionsWithEventThisTick.push_back(action);
                    }

                    InputActionValue rawValue = GetKeyState(key)->RawValue;
                    InputActionValue modifiedValue = ApplyModifiers(mapping.GetModifiers(), rawValue, deltaTime);
                    instance->Value += modifiedValue;   // we directly accumulate values from multiple mappings. TODO: maybe distinguish different behaviors of accumulation later
                
                    // Evaluate trigger state
                    InputTriggerState newState = triggerStateTracker.EvaluateTriggerState(mapping.GetTriggers(), modifiedValue, deltaTime);
                    
                    // handle no trigger situation. Same behavior as InputTriggerDown
                    triggerStateTracker.SetNoTriggerState(modifiedValue.IsNonZero() ? InputTriggerState::Triggered : InputTriggerState::None);

                    instance->TriggerStateTracker = std::max(instance->TriggerStateTracker, triggerStateTracker);
                }
                
            }
        }

        // Apply action modifiers and evaluate triggers. Then finalize action instances
        for(auto& [action, instance] : m_ActionInstances)
        {
            bool bActionUpdatedThisFrame = !(m_ActionsWithEventThisTick.end() == 
                    std::find(m_ActionsWithEventThisTick.begin(), m_ActionsWithEventThisTick.end(), action));

            if(bActionUpdatedThisFrame)
            {
                InputActionValue modifiedValue = ApplyModifiers(action->GetModifiers(), instance.Value, deltaTime);
                instance.Value = modifiedValue;

                InputTriggerState prevState = instance.TriggerStateTracker.GetState();

                instance.TriggerStateTracker.SetNoTriggerState(modifiedValue.IsNonZero() ? InputTriggerState::Triggered : InputTriggerState::None);
                InputTriggerState newState = instance.TriggerStateTracker.EvaluateTriggerState(action->GetTriggers(), modifiedValue, deltaTime);
                
                instance.TriggerEvent = GetTriggerStateChangeEvent(prevState, newState);
                instance.LastState = newState;
            }
        }

        // Notify Input Components
        for(auto* inputComponent : m_InputComponents)
        {
            for(auto* action : m_ActionsWithEventThisTick)
            {
                InputActionInstance* instance = FindInputActionInstance(action);
                if(instance)
                {
                    inputComponent->ProcessInputAction(action, instance->TriggerEvent, instance->Value);
                }
            }
        }

        // Special handling for scroll input to reset its state after processing to avoid continuous scroll input
        InputKeyState* mouseScrollState = GetKeyState(InputKeys::MouseScroll);
        if(mouseScrollState->action == InputKeyAction::Down)
        {
            // Reset scroll state after processing to avoid continuous scroll input
            mouseScrollState->action = InputKeyAction::Idle;
            mouseScrollState->RawValue = Vector3(0.0f, 0.0f, 0.0f);
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
        auto it = Get().m_KeyStateMap.find(InputKeys::Mouse2D);
        if(it != Get().m_KeyStateMap.end())        
        {
            return Vector2(it->second.RawValue.x, it->second.RawValue.y);
        }
        return Vector2(0.0f, 0.0f);
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
        auto it = m_KeyStateMap.find(InputKeys::Mouse2D);
        if(it != m_KeyStateMap.end())
        {
            InputKeyState& keyState = it->second;

            keyState.action = (Math::abs(xPos) > 0.1f || Math::abs(yPos) > 0.1f) ? InputKeyAction::Down : InputKeyAction::Idle;
            keyState.RawValue = Vector3(static_cast<float>(xPos), static_cast<float>(yPos), 0.0f);
        }
    }

    void InputSystem::OnMouseScroll(double xOffset, double yOffset)
    {
        InputKeyState* scrollState = GetKeyState(InputKeys::MouseScroll);

        scrollState->action = InputKeyAction::Idle;
        scrollState->RawValue = Vector3(0.0f, 0.0f, 0.0f);

        scrollState->action = InputKeyAction::Down;
        scrollState->RawValue = Vector3(static_cast<float>(xOffset), static_cast<float>(yOffset), 0.0f);
    }

    void InputSystem::OnKeyOrMouseButton_Internal(InputKey key, int scancode, InputKeyAction action, int mods)
    {
        auto it = m_KeyStateMap.find(key);
        if(it != m_KeyStateMap.end())
        {
            InputKeyState& keyState = it->second;
            InputKeyAction prevAction = keyState.action;
            
            keyState.action = CalculateKeyAction(prevAction, action);
            keyState.RawValue = keyState.action & (InputKeyAction::Press | InputKeyAction::Down) ? Vector3(1.0f, 0.0f, 0.0f) : Vector3(0.0f, 0.0f, 0.0f);
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