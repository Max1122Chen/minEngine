#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>

namespace minEngine
{
    class MaterialInstance;
    class Asset;
    class AssetManager;

    class MaterialInstanceLoader
    {
    public:
        /** Deserialize .memi and finalize Parent/proxy. Does not compile. */
        static std::shared_ptr<MaterialInstance> Load(const AssetMeta& meta, std::string* outError = nullptr);
        static bool Save(const AssetMeta& meta, const MaterialInstance& asset, std::string* outError = nullptr);
        static std::shared_ptr<Asset> CreateHandler(
            AssetManager& manager,
            const std::string& assetName,
            const std::string& directoryRel,
            std::string& outError);
        static bool SaveHandler(
            const AssetManager& manager,
            const AssetMeta& meta,
            const Asset& asset,
            std::string& outError);
    };
}
