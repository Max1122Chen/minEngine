#pragma once

#include "Runtime/Function/Framework/Scene/SceneManager.h"

namespace minEngine::Testing
{
    template<>
    class TestAccess<SceneManager>
    {
    public:
        static void SetInstance(SceneManager* instance)
        {
            SceneManager::SetInstance(instance);
        }
    };
}
