#pragma once
#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"

namespace minEngine
{
    ME_STRUCT()
    struct AssetMeta
    {
        ME_REFLECTION_FRIEND(AssetMeta)

        ME_PROPERTY()
        std::string AssetPath;

        ME_PROPERTY()
        std::string AssetType; // e.g., "Texture2D", "StaticMesh", "Scene"
        
        GUID Guid;
    };
}

#include "Generated/Reflection/AssetMeta.gen.h"