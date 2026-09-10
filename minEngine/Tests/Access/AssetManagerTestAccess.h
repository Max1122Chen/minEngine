#pragma once

#include "Runtime/Resource/AssetManager.h"

namespace minEngine::Testing
{
    template<>
    class TestAccess<AssetManager>
    {
    public:
        static void SetInstance(AssetManager* instance)
        {
            AssetManager::SetInstance(instance);
        }
    };
}
