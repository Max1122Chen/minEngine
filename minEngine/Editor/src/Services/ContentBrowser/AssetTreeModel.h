#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetRegistryTypes.h"
#include "Runtime/Resource/AssetMeta.h"

#include <filesystem>
#include <string>
#include <vector>

namespace minEngine
{
    class AssetTreeModel
    {
    public:
        struct DirectoryNode
        {
            std::string RelativePath;
            std::string DisplayName;
            std::vector<const AssetMeta*> Assets;
            std::vector<DirectoryNode> Children;
        };

        void ResetForProject(const std::filesystem::path& projectContentRoot);
        void Clear();

        void SetCurrentDirectory(std::string_view projectRelativeDir);
        std::string_view GetCurrentDirectory() const;

        const DirectoryNode& GetDirectoryTreeRoot() const;
        std::vector<const DirectoryNode*> GetSubdirectoriesInCurrentDirectory() const;
        const std::vector<const AssetMeta*>& GetAssetsInCurrentDirectory() const;

        void OnRegistryChange(const AssetRegistryChange& change);
        void RebuildDirectoryTree();
        void RebuildCurrentDirectoryAssetList();

        uint32_t SubscribeToAssetManager();
        void UnsubscribeFromAssetManager();

    private:
        static std::string NormalizeParentDirectoryRel(std::string_view assetRelativePath);
        static std::string NormalizeDirectoryRel(std::string_view projectRelativeDirectory);

        bool IsAssetInCurrentDirectory(std::string_view assetRelativePath) const;
        bool IsChangeRelevantToCurrentDirectory(const AssetRegistryChange& change) const;

        DirectoryNode* FindDirectoryNode(std::string_view directoryRel);
        const DirectoryNode* FindDirectoryNode(std::string_view directoryRel) const;
        DirectoryNode* GetOrInsertDirectoryNode(std::string_view directoryRel);

        void InsertAssetSorted(std::vector<const AssetMeta*>& assets, const AssetMeta* meta);
        bool RemoveAssetByPath(std::vector<const AssetMeta*>& assets, std::string_view assetPath);

        void ApplyRegisteredChange(const AssetRegistryChange& change);
        void ApplyUnregisteredChange(const AssetRegistryChange& change);
        void ApplyMovedChange(const AssetRegistryChange& change);
        void ApplyMetaUpdatedChange(const AssetRegistryChange& change);

        void InsertIntoCurrentDirectoryList(const AssetMeta* meta);
        void RemoveFromCurrentDirectoryList(std::string_view assetPath);

        void BuildDirectoryNodeRecursive(
            DirectoryNode& outNode,
            const std::filesystem::path& absoluteDirectory);

        std::filesystem::path m_ContentRoot;
        std::string m_CurrentDirectoryRel;
        DirectoryNode m_TreeRoot;
        std::vector<const AssetMeta*> m_CurrentAssets;
        uint32_t m_RegistrySubscriptionId = kInvalidAssetRegistrySubscriptionId;
    };
}
