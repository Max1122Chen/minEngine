#pragma once

#include "Runtime/Function/Input/InputSystem.h"

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
    };
}
