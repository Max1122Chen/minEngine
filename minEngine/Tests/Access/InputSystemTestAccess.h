#pragma once

#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Input/InputKeys.h"
#include "Runtime/Function/Input/InputKeyState.h"

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

        static void Tick(InputSystem& input, float deltaTime)
        {
            input.Tick(deltaTime);
        }

        static bool IsMouseScrollDown(const InputSystem& input)
        {
            return input.GetKeyState(InputKeys::MouseScroll)->action == InputKeyAction::Down;
        }
    };
}