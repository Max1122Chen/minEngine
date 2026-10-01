#pragma once

#include "Runtime/Resource/AssetManager.h"

#include <string_view>

namespace minEngine
{
    class Texture2DImporter
    {
    public:
        static bool AcceptsSourceExtension(std::string_view extension);
        static ImportResult Import(AssetManager& manager, const ImportRequest& request);

        static bool RecookFromMeta(
            AssetManager& manager,
            const AssetMeta& meta,
            std::string_view channelSemanticsOverride,
            std::string& outError);
    };
}
