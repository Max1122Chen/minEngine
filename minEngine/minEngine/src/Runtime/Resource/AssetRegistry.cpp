#include "Runtime/Resource/AssetRegistry.h"

#include <algorithm>

namespace minEngine
{
    void AssetRegistry::ClearRegistryData()
    {
        m_BatchDepth = 0;
        m_PendingChanges.clear();

        m_MetasByParentDir.clear();
        m_MetasByType.clear();
        m_PathByGuid.clear();
        m_Metas.clear();
    }

    void AssetRegistry::Shutdown()
    {
        ClearRegistryData();
        m_Subscribers.clear();
        m_NextSubscriptionId = 1u;
    }

    void AssetRegistry::BeginBatch()
    {
        ++m_BatchDepth;
    }

    void AssetRegistry::EndBatch()
    {
        ME_ASSERT(m_BatchDepth > 0, "AssetRegistry::EndBatch without matching BeginBatch");
        --m_BatchDepth;

        if (m_BatchDepth > 0)
        {
            return;
        }

        if (m_PendingChanges.empty())
        {
            return;
        }

        const std::vector<AssetRegistryChange> changes = std::move(m_PendingChanges);
        m_PendingChanges.clear();

        for (const AssetRegistryChange& change : changes)
        {
            BroadcastChange(change);
        }
    }

    uint32_t AssetRegistry::Subscribe(AssetRegistryChangedCallback callback)
    {
        if (!callback)
        {
            return kInvalidAssetRegistrySubscriptionId;
        }

        const uint32_t subscriptionId = m_NextSubscriptionId++;
        m_Subscribers[subscriptionId] = std::move(callback);
        return subscriptionId;
    }

    void AssetRegistry::Unsubscribe(uint32_t subscriptionId)
    {
        if (subscriptionId == kInvalidAssetRegistrySubscriptionId)
        {
            return;
        }

        m_Subscribers.erase(subscriptionId);
    }

    bool AssetRegistry::ContainsPath(std::string_view projectRelativePath) const
    {
        return m_Metas.find(std::string(projectRelativePath)) != m_Metas.end();
    }

    bool AssetRegistry::ContainsGuid(const GUID& guid) const
    {
        return m_PathByGuid.find(guid) != m_PathByGuid.end();
    }

    const AssetMeta* AssetRegistry::FindMetaByPath(std::string_view projectRelativeOrLegacyPath) const
    {
        auto iter = m_Metas.find(std::string(projectRelativeOrLegacyPath));
        if (iter == m_Metas.end())
        {
            return nullptr;
        }
        return &iter->second;
    }

    const AssetMeta* AssetRegistry::FindMetaByGuid(const GUID& guid) const
    {
        auto guidIter = m_PathByGuid.find(guid);
        if (guidIter == m_PathByGuid.end())
        {
            return nullptr;
        }

        return FindMetaByPath(guidIter->second);
    }

    std::vector<const AssetMeta*> AssetRegistry::FindMetasByType(std::string_view assetTypeId) const
    {
        std::vector<const AssetMeta*> result;
        auto bucketIter = m_MetasByType.find(std::string(assetTypeId));
        if (bucketIter == m_MetasByType.end())
        {
            return result;
        }

        result.reserve(bucketIter->second.size());
        for (AssetMeta* meta : bucketIter->second)
        {
            result.push_back(meta);
        }

        return result;
    }

    std::string AssetRegistry::NormalizeDirectoryRel(std::string_view projectRelativeDirectory)
    {
        std::string directoryRel(projectRelativeDirectory);
        if (directoryRel.empty())
        {
            return std::string();
        }

        return std::filesystem::path(directoryRel).lexically_normal().generic_string();
    }

    std::vector<const AssetMeta*> AssetRegistry::FindMetasUnderDirectory(
        std::string_view projectRelativeDirectory) const
    {
        const std::string directoryRel = NormalizeDirectoryRel(projectRelativeDirectory);

        std::vector<const AssetMeta*> result;
        auto dirIter = m_MetasByParentDir.find(directoryRel);
        if (dirIter == m_MetasByParentDir.end())
        {
            return result;
        }

        const std::vector<AssetMeta*>& bucket = dirIter->second;
        result.reserve(bucket.size());
        for (AssetMeta* meta : bucket)
        {
            result.push_back(meta);
        }

        std::sort(
            result.begin(),
            result.end(),
            [](const AssetMeta* left, const AssetMeta* right)
            {
                return left->AssetName < right->AssetName;
            });

        return result;
    }

    void AssetRegistry::RemoveFromTypeBucket(const AssetMeta& meta)
    {
        auto bucketIter = m_MetasByType.find(meta.AssetType);
        if (bucketIter == m_MetasByType.end())
        {
            return;
        }

        std::vector<AssetMeta*>& bucket = bucketIter->second;
        auto registryIter = m_Metas.find(meta.AssetPath);
        if (registryIter == m_Metas.end())
        {
            return;
        }

        AssetMeta* target = &registryIter->second;
        bucket.erase(
            std::remove(bucket.begin(), bucket.end(), target),
            bucket.end());

        if (bucket.empty())
        {
            m_MetasByType.erase(bucketIter);
        }
    }

    void AssetRegistry::AddToTypeBucket(const AssetMeta& meta)
    {
        auto registryIter = m_Metas.find(meta.AssetPath);
        if (registryIter == m_Metas.end())
        {
            return;
        }

        m_MetasByType[meta.AssetType].push_back(&registryIter->second);
    }

    void AssetRegistry::RemoveFromDirectoryIndex(const AssetMeta& meta)
    {
        const std::string parentRel =
            std::filesystem::path(meta.AssetPath).parent_path().lexically_normal().generic_string();

        auto bucketIter = m_MetasByParentDir.find(parentRel);
        if (bucketIter == m_MetasByParentDir.end())
        {
            return;
        }

        std::vector<AssetMeta*>& bucket = bucketIter->second;
        auto registryIter = m_Metas.find(meta.AssetPath);
        if (registryIter == m_Metas.end())
        {
            return;
        }

        AssetMeta* target = &registryIter->second;
        bucket.erase(
            std::remove(bucket.begin(), bucket.end(), target),
            bucket.end());

        if (bucket.empty())
        {
            m_MetasByParentDir.erase(bucketIter);
        }
    }

    void AssetRegistry::AddToDirectoryIndex(const AssetMeta& meta)
    {
        auto registryIter = m_Metas.find(meta.AssetPath);
        if (registryIter == m_Metas.end())
        {
            return;
        }

        const std::string parentRel =
            std::filesystem::path(meta.AssetPath).parent_path().lexically_normal().generic_string();

        m_MetasByParentDir[parentRel].push_back(&registryIter->second);
    }

    void AssetRegistry::BroadcastChange(const AssetRegistryChange& change)
    {
        for (const auto& [subscriptionId, callback] : m_Subscribers)
        {
            (void)subscriptionId;
            if (callback)
            {
                callback(change);
            }
        }
    }

    void AssetRegistry::EnqueueOrBroadcastChange(const AssetRegistryChange& change)
    {
        if (m_BatchDepth > 0)
        {
            m_PendingChanges.push_back(change);
            return;
        }

        BroadcastChange(change);
    }

    void AssetRegistry::CacheMeta(const AssetMeta& meta, bool alreadyRegistered)
    {
        auto existingIter = m_Metas.find(meta.AssetPath);
        if (existingIter != m_Metas.end())
        {
            RemoveFromTypeBucket(existingIter->second);
            RemoveFromDirectoryIndex(existingIter->second);
        }

        m_Metas[meta.AssetPath] = meta;
        m_PathByGuid[meta.Guid] = meta.AssetPath;
        AddToTypeBucket(meta);
        AddToDirectoryIndex(meta);

        AssetRegistryChange change;
        change.Kind = alreadyRegistered ? AssetRegistryChangeKind::MetaUpdated
                                        : AssetRegistryChangeKind::Registered;
        change.Guid = meta.Guid;
        change.NewPath = meta.AssetPath;
        change.AssetTypeId = meta.AssetType;
        EnqueueOrBroadcastChange(change);
    }

    void AssetRegistry::UncacheMeta(std::string_view projectRelativePath)
    {
        const std::string key(projectRelativePath);
        auto registryIter = m_Metas.find(key);
        if (registryIter == m_Metas.end())
        {
            return;
        }

        const AssetMeta meta = registryIter->second;
        RemoveFromTypeBucket(meta);
        RemoveFromDirectoryIndex(meta);
        m_PathByGuid.erase(meta.Guid);
        m_Metas.erase(registryIter);

        AssetRegistryChange change;
        change.Kind = AssetRegistryChangeKind::Unregistered;
        change.Guid = meta.Guid;
        change.OldPath = meta.AssetPath;
        change.AssetTypeId = meta.AssetType;
        EnqueueOrBroadcastChange(change);
    }

    bool AssetRegistry::MoveMeta(std::string_view oldRel, std::string_view newRel)
    {
        const std::string oldKey(oldRel);
        const std::string newKey(newRel);

        auto oldIter = m_Metas.find(oldKey);
        if (oldIter == m_Metas.end())
        {
            return false;
        }

        if (m_Metas.find(newKey) != m_Metas.end())
        {
            return false;
        }

        AssetMeta meta = oldIter->second;
        RemoveFromTypeBucket(meta);
        RemoveFromDirectoryIndex(meta);
        m_Metas.erase(oldIter);

        meta.AssetPath = newKey;
        meta.AssetName = std::filesystem::path(newKey).stem().string();

        m_Metas.emplace(newKey, meta);
        m_PathByGuid[meta.Guid] = newKey;
        AddToTypeBucket(meta);
        AddToDirectoryIndex(meta);

        AssetRegistryChange change;
        change.Kind = AssetRegistryChangeKind::Moved;
        change.Guid = meta.Guid;
        change.OldPath = oldKey;
        change.NewPath = newKey;
        change.AssetTypeId = meta.AssetType;
        EnqueueOrBroadcastChange(change);

        return true;
    }
}

