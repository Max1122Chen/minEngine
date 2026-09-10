#pragma once

#include "Runtime/Function/Physics/PhysicsSystem.h"

namespace minEngine::Testing
{
    template<>
    class TestAccess<PhysicsSystem>
    {
    public:
        static void SetInstance(PhysicsSystem* instance)
        {
            PhysicsSystem::SetInstance(instance);
        }
    };
}
