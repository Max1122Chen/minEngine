#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>

namespace minEngine
{
    class Material;
    class Asset;
    class AssetManager;

    class MaterialLoader
    {
    public:
        /** Deserialize .memtl, finalize graph, compile GPU material. Returns nullptr on any failure. */
        static std::shared_ptr<Material> Load(const AssetMeta& meta, std::string* outError = nullptr);
        static bool Save(const AssetMeta& meta, const Material& asset, std::string* outError = nullptr);
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
