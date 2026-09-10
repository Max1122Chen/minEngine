#pragma once

#include "Runtime/Core/Object/ObjectManager.h"

namespace minEngine::Testing
{
    template<>
    class TestAccess<ObjectManager>
    {
    public:
        static void SetInstance(ObjectManager* instance)
        {
            ObjectManager::SetInstance(instance);
        }
    };
}
