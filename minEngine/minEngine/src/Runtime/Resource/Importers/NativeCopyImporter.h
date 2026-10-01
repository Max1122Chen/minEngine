#pragma once

#include "Runtime/Resource/AssetManager.h"

#include <filesystem>
#include <string_view>

namespace minEngine
{
    /** ASSET-F06: copy registered native extensions into Content (no cook). */
    class NativeCopyImporter
    {
    public:
        static bool AcceptsSourceExtension(std::string_view extension);
        static ImportResult Import(AssetManager& manager, const ImportRequest& request);

    private:
        static ImportAssetResult Cook(
            AssetManager& manager,
            const std::filesystem::path& sourcePath,
            const std::filesystem::path& destDirectory,
            bool bOverwriteExisting);
    };
}
