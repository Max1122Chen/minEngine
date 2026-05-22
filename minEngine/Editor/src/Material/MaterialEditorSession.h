#pragma once

#include "Core.h"

#include "Runtime/Function/Render/Material.h"

#include <memory>
#include <string>

namespace minEngine
{
    struct MaterialEditorSession
    {
        std::shared_ptr<Material> MaterialAsset;
        std::string AssetPath;
        bool Dirty = false;

        bool HasOpenMaterial() const
        {
            return MaterialAsset != nullptr && !AssetPath.empty();
        }

        void Clear()
        {
            MaterialAsset.reset();
            AssetPath.clear();
            Dirty = false;
        }
    };
}
