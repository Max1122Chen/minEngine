#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"
#include "Runtime/Resource/MeshLoader.h"

#include <memory>

namespace minEngine
{
    class StaticMesh;

    class StaticMeshLoader
    {
    public:
        static std::shared_ptr<StaticMesh> CreateFromImportData(
            const AssetMeta& meta,
            MeshImportData& importData);

        static std::shared_ptr<StaticMesh> LoadFromAssetMeta(const AssetMeta& meta);
    };
}
