#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>

namespace minEngine
{
    class Shader;

    class ShaderLoader
    {
    public:
        static std::shared_ptr<Shader> LoadFromAssetMeta(const AssetMeta& meta);
    };
}
