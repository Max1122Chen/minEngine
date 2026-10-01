#pragma once

#include "Runtime/Resource/AssetManager.h"

#include <filesystem>
#include <string_view>

namespace minEngine
{
    enum class MeshImportProductType
    {
        StaticMesh,
        SkeletalMesh
    };

    /** ASSET-F06: external mesh cook (obj/glb) + Register + SourcePath meta. */
    class MeshImporter
    {
    public:
        static bool AcceptsSourceExtension(std::string_view extension);
        static ImportResult ImportStaticMesh(AssetManager& manager, const ImportRequest& request);
        static ImportResult ImportSkeletalMesh(AssetManager& manager, const ImportRequest& request);

    private:
        static ImportAssetResult Cook(
            AssetManager& manager,
            const std::filesystem::path& sourcePath,
            const std::filesystem::path& destDirectory,
            MeshImportProductType productType,
            bool bOverwriteExisting);
    };
}
