#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    namespace Reflection
    {
        class MEClass;
    }

    class Asset;
    class AssetManager;
    class DataAsset;

    class DataAssetLoader
    {
    public:
        static std::shared_ptr<DataAsset> Load(const AssetMeta& meta, std::string* outError = nullptr);
        static bool Save(const AssetMeta& meta, const DataAsset& asset, std::string* outError = nullptr);
        static bool SaveHandler(
            const AssetManager& manager,
            const AssetMeta& meta,
            const Asset& asset,
            std::string& outError);
        static std::shared_ptr<Asset> CreateHandler(
            AssetManager& manager,
            const std::string& assetName,
            const std::string& directoryRel,
            std::string& outError);
        static std::shared_ptr<Asset> Create(
            AssetManager& manager,
            const Reflection::MEClass* classInfo,
            const std::string& assetName,
            const std::string& directoryRel,
            std::string& outError);

        /**
         * Resolve a reflected class for CreateDataAsset.
         * Empty className selects the DataAsset base class. Non-empty must FindClass,
         * IsA(DataAsset), and not Abstract.
         */
        static MINENGINE_API const Reflection::MEClass* ResolveCreatableClass(
            std::string_view runtimeClassName,
            std::string& outError);

        /** DataAsset + non-Abstract derived classes currently in the reflection table. */
        static MINENGINE_API std::vector<const Reflection::MEClass*> CollectCreatableClasses();
    };
}
