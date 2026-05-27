#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"
#include "Runtime/Resource/AssetRegistryTypes.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    class AssetRegistry
    {
    public:
        AssetRegistry() = default;
        ~AssetRegistry() = default;

        void ClearRegistryData();
        void Shutdown();

        void BeginBatch();
        void EndBatch();

        uint32_t Subscribe(AssetRegistryChangedCallback callback);
        void Unsubscribe(uint32_t subscriptionId);

        const AssetMeta* FindMetaByPath(std::string_view projectRelativeOrLegacyPath) const;
        const AssetMeta* FindMetaByGuid(const GUID& guid) const;
        std::vector<const AssetMeta*> FindMetasByType(std::string_view assetTypeId) const;
        std::vector<const AssetMeta*> FindMetasUnderDirectory(std::string_view projectRelativeDirectory) const;

        bool ContainsPath(std::string_view projectRelativePath) const;
        bool ContainsGuid(const GUID& guid) const;

        void CacheMeta(const AssetMeta& meta, bool alreadyRegistered);
        void UncacheMeta(std::string_view projectRelativePath);
        bool MoveMeta(std::string_view oldRel, std::string_view newRel);

    private:
        void RemoveFromTypeBucket(const AssetMeta& meta);
        void AddToTypeBucket(const AssetMeta& meta);

        void RemoveFromDirectoryIndex(const AssetMeta& meta);
        void AddToDirectoryIndex(const AssetMeta& meta);

        void BroadcastChange(const AssetRegistryChange& change);
        void EnqueueOrBroadcastChange(const AssetRegistryChange& change);

        static std::string NormalizeDirectoryRel(std::string_view projectRelativeDirectory);

    private:
        std::unordered_map<std::string, AssetMeta> m_Metas;
        std::unordered_map<GUID, std::string, GUID::Hash> m_PathByGuid;
        std::unordered_map<std::string, std::vector<AssetMeta*>> m_MetasByType;
        std::unordered_map<std::string, std::vector<AssetMeta*>> m_MetasByParentDir;

        std::unordered_map<uint32_t, AssetRegistryChangedCallback> m_Subscribers;
        uint32_t m_NextSubscriptionId = 1u;

        int m_BatchDepth = 0;
        std::vector<AssetRegistryChange> m_PendingChanges;
    };
}

