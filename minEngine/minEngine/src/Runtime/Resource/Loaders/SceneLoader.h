#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>

namespace minEngine
{
    class Scene;

    class SceneLoader
    {
    public:
        static std::shared_ptr<Scene> Load(const AssetMeta& meta);
    };
}
