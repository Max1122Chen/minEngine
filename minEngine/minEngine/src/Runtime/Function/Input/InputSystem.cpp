#include "InputSystem.h"

#include "InputKeys.h"
#include "InputKeyState.h"

#include "InputMappingContext.h"

#include "InputModifiers.h"
#include "Framework/Components/InputComponent.h"

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
        WindowSystem* windowSystem = RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem.get();

        // TODO: maybe we will wrap these logic into a private function later
        windowSystem->RegisterOnKeyCallback([this](int key, int scancode, int action, int mods)
        {
            this->OnKey(key, scancode, action, mods);
        });

        windowSystem->RegisterOnCursorPosCallback([this](double xPos, double yPos)
        {
            this->OnCursorPos(xPos, yPos);
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
                bool bShouldResetAction = (m_ActionsWithEventThisTick.end() == 
                    std::find(m_ActionsWithEventThisTick.begin(), m_ActionsWithEventThisTick.end(), action));

                const InputKey& key = mapping.Key;
                if(GetKeyState(key)->bDown)
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

    void InputSystem::OnKey(int key, int scancode, int action, int mods)
    {
        InputKey inputKey = m_InputKeys.ConvertGLFWKeyToInputKey(key);
        auto it = m_KeyStateMap.find(inputKey);
        if(it != m_KeyStateMap.end())
        {
            InputKeyState& keyState = it->second;
            
            keyState.bDown = (action != GLFW_RELEASE);
            keyState.RawValue = keyState.bDown ? Vector3(1.0f, 0.0f, 0.0f) : Vector3(0.0f, 0.0f, 0.0f);
        }
    }

    void InputSystem::OnCursorPos(double xPos, double yPos)
    {
        double deltaX = xPos - m_LastCursorX;
        double deltaY = m_LastCursorY - yPos; // Invert Y axis

        auto it = m_KeyStateMap.find(InputKeys::Mouse2D);
        if(it != m_KeyStateMap.end())
        {
            InputKeyState& keyState = it->second;

            keyState.bDown = (Math::abs(deltaX) > 0.1f || Math::abs(deltaY) > 0.1f);
            keyState.RawValue = Vector3(static_cast<float>(deltaX), static_cast<float>(deltaY), 0.0f);
        }
        

        m_LastCursorX = xPos;;
        m_LastCursorY = yPos;
    }


    InputActionValue InputSystem::ApplyModifiers(const std::vector<std::shared_ptr<InputModifier>> &modifiers, const InputActionValue &rawValue, float deltaTime)
    {
        InputActionValue modifiedValue = rawValue;
        for(auto modifier : modifiers)
        {
            modifiedValue = modifier->ModifyRaw(modifiedValue, deltaTime);
        }
        return modifiedValue;return InputActionValue();
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