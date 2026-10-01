#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>

namespace minEngine
{
    class Prefab;
    class Asset;
    class AssetManager;

    class PrefabLoader
    {
    public:
        static std::shared_ptr<Prefab> Load(const AssetMeta& meta, std::string* outError = nullptr);
        static bool Save(const AssetMeta& meta, const Prefab& asset, std::string* outError = nullptr);
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
