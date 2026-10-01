#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>

namespace minEngine
{
    class Scene;
    class Asset;
    class AssetManager;

    class SceneLoader
    {
    public:
        static std::shared_ptr<Scene> Load(const AssetMeta& meta);
        static bool Save(const AssetMeta& meta, const Scene& asset, std::string* outError = nullptr);
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
