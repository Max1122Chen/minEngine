#pragma once

#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Input/InputKeys.h"
#include "Runtime/Function/Input/InputKeyState.h"
#include "Runtime/Function/Input/InputTriggers.h"
#include "Runtime/Function/Input/InputAction.h"

namespace minEngine::Testing
{
    template<>
    class TestAccess<InputSystem>
    {
    public:
        static void SetInstance(InputSystem* instance)
        {
            InputSystem::SetInstance(instance);
        }

        static void SimulateMouseScroll(InputSystem& input, float yOffset)
        {
            input.OnMouseScroll(0.0, static_cast<double>(yOffset));
        }

        static void SimulateKey(InputSystem& input, const InputKey& key, InputKeyAction action)
        {
            input.OnKey(key, 0, action, 0);
        }

        static InputKeyAction GetKeyAction(const InputSystem& input, const InputKey& key)
        {
            return input.GetKeyState(key)->action;
        }

        static bool KeyDown(const InputSystem& input, const InputKey& key)
        {
            return (input.GetKeyState(key)->action & (InputKeyAction::Press | InputKeyAction::Down)) != 0;
        }

        static void Tick(InputSystem& input, float deltaTime)
        {
            input.Tick(deltaTime);
        }

        static bool IsMouseScrollDown(const InputSystem& input)
        {
            return input.GetKeyState(InputKeys::MouseScroll)->action == InputKeyAction::Down;
        }

        static InputTriggerEvent GetTriggerEvent(InputSystem& input, InputAction* action)
        {
            InputActionInstance* instance = input.FindInputActionInstance(action);
            return instance != nullptr ? instance->TriggerEvent : InputTriggerEvent::None;
        }

        static InputTriggerState GetLastTriggerState(InputSystem& input, InputAction* action)
        {
            InputActionInstance* instance = input.FindInputActionInstance(action);
            return instance != nullptr ? instance->LastState : InputTriggerState::None;
        }
    };
}
