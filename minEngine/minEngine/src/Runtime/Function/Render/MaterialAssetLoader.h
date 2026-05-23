#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>

namespace minEngine
{
    class Material;

    class MaterialAssetLoader
    {
    public:
        static std::shared_ptr<Material> LoadFromAssetMeta(const AssetMeta& meta);
    };
}
