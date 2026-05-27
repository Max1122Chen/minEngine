#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <string>
#include <vector>

namespace minEngine
{
    enum class ContentBrowserHitKind
    {
        TreeDirectory,
        TreeAsset,
        TileAsset,
        ListBackground,
    };

    struct ContentBrowserMenuContext
    {
        ContentBrowserHitKind HitKind = ContentBrowserHitKind::ListBackground;
        std::string CurrentDirectoryRel;
        std::vector<const AssetMeta*> SelectedAssets;
    };

} // namespace minEngine
