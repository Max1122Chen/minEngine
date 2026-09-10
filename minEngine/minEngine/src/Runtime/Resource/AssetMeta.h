#pragma once
#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"

namespace minEngine
{
    ME_STRUCT()
    struct AssetMeta
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        std::string AssetName;

        ME_PROPERTY()
        std::string AssetPath;

        ME_PROPERTY()
        std::string AssetType; // e.g., "Texture2D", "StaticMesh", "Scene"

        // Project-relative Import Source path (e.g. Assets/Sources/Hero.fbx). Empty if native-authored.
        ME_PROPERTY()
        std::string SourcePath;

        ME_PROPERTY()
        GUID Guid;
    };
}

#include "AssetMeta.gen.h"