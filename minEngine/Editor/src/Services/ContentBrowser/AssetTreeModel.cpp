#include "Services/ContentBrowser/AssetTreeModel.h"

#include "Runtime/Resource/AssetManager.h"

#include <algorithm>

namespace minEngine
{
    void AssetTreeModel::ResetForProject(const std::filesystem::path& projectContentRoot)
    {
        Clear();

        if (projectContentRoot.empty())
        {
            return;
        }

        m_ContentRoot = std::filesystem::weakly_canonical(projectContentRoot);
        m_CurrentDirectoryRel.clear();
        RebuildDirectoryTree();
        RebuildCurrentDirectoryAssetList();
        SubscribeToAssetManager();
    }

    void AssetTreeModel::Clear()
    {
        UnsubscribeFromAssetManager();
        m_ContentRoot.clear();
        m_CurrentDirectoryRel.clear();
        m_TreeRoot = DirectoryNode();
        m_CurrentAssets.clear();
    }

    void AssetTreeModel::SetCurrentDirectory(std::string_view projectRelativeDir)
    {
        if (projectRelativeDir.empty())
        {
            m_CurrentDirectoryRel.clear();
        }
        else
        {
            m_CurrentDirectoryRel =
                std::filesystem::path(projectRelativeDir).lexically_normal().generic_string();
        }

        RebuildCurrentDirectoryAssetList();
    }

    std::string_view AssetTreeModel::GetCurrentDirectory() const
    {
        return m_CurrentDirectoryRel;
    }

    const AssetTreeModel::DirectoryNode& AssetTreeModel::GetDirectoryTreeRoot() const
    {
        return m_TreeRoot;
    }

    const std::vector<const AssetMeta*>& AssetTreeModel::GetAssetsInCurrentDirectory() const
    {
        return m_CurrentAssets;
    }

    uint32_t AssetTreeModel::SubscribeToAssetManager()
    {
        if (!AssetManager::HasInstance())
        {
            return kInvalidAssetRegistrySubscriptionId;
        }

        if (m_RegistrySubscriptionId != kInvalidAssetRegistrySubscriptionId)
        {
            return m_RegistrySubscriptionId;
        }

        m_RegistrySubscriptionId = AssetManager::Get().Subscribe(
            [this](const AssetRegistryChange& change)
            {
                OnRegistryChange(change);
            });

        return m_RegistrySubscriptionId;
    }

    void AssetTreeModel::UnsubscribeFromAssetManager()
    {
        if (m_RegistrySubscriptionId == kInvalidAssetRegistrySubscriptionId || !AssetManager::HasInstance())
        {
            m_RegistrySubscriptionId = kInvalidAssetRegistrySubscriptionId;
            return;
        }

        AssetManager::Get().Unsubscribe(m_RegistrySubscriptionId);
        m_RegistrySubscriptionId = kInvalidAssetRegistrySubscriptionId;
    }

    std::string AssetTreeModel::NormalizeParentDirectoryRel(std::string_view assetRelativePath)
    {
        const std::string parentRel =
            std::filesystem::path(assetRelativePath).parent_path().lexically_normal().generic_string();
        if (parentRel == ".")
        {
            return std::string();
        }
        return parentRel;
    }

    std::string AssetTreeModel::NormalizeDirectoryRel(std::string_view projectRelativeDirectory)
    {
        if (projectRelativeDirectory.empty())
        {
            return std::string();
        }

        return std::filesystem::path(projectRelativeDirectory).lexically_normal().generic_string();
    }

    AssetTreeModel::DirectoryNode* AssetTreeModel::FindDirectoryNode(std::string_view directoryRel)
    {
        const std::string normalizedDir = NormalizeDirectoryRel(directoryRel);
        if (normalizedDir.empty())
        {
            return &m_TreeRoot;
        }

        std::vector<std::string> segments;
        for (std::filesystem::path segment : std::filesystem::path(normalizedDir))
        {
            if (!segment.empty() && segment != ".")
            {
                segments.push_back(segment.generic_string());
            }
        }

        DirectoryNode* current = &m_TreeRoot;
        std::string accumulatedRel;

        for (const std::string& segment : segments)
        {
            if (!accumulatedRel.empty())
            {
                accumulatedRel += '/';
            }
            accumulatedRel += segment;

            DirectoryNode* childMatch = nullptr;
            for (DirectoryNode& child : current->Children)
            {
                if (child.RelativePath == accumulatedRel)
                {
                    childMatch = &child;
                    break;
                }
            }

            if (childMatch == nullptr)
            {
                return nullptr;
            }

            current = childMatch;
        }

        return current;
    }

    const AssetTreeModel::DirectoryNode* AssetTreeModel::FindDirectoryNode(
        std::string_view directoryRel) const
    {
        return const_cast<AssetTreeModel*>(this)->FindDirectoryNode(directoryRel);
    }

    AssetTreeModel::DirectoryNode* AssetTreeModel::GetOrInsertDirectoryNode(std::string_view directoryRel)
    {
        const std::string normalizedDir = NormalizeDirectoryRel(directoryRel);
        if (normalizedDir.empty())
        {
            return &m_TreeRoot;
        }

        std::vector<std::string> segments;
        for (std::filesystem::path segment : std::filesystem::path(normalizedDir))
        {
            if (!segment.empty() && segment != ".")
            {
                segments.push_back(segment.generic_string());
            }
        }

        DirectoryNode* current = &m_TreeRoot;
        std::string accumulatedRel;

        for (const std::string& segment : segments)
        {
            if (!accumulatedRel.empty())
            {
                accumulatedRel += '/';
            }
            accumulatedRel += segment;

            DirectoryNode* childMatch = nullptr;
            for (DirectoryNode& child : current->Children)
            {
                if (child.RelativePath == accumulatedRel)
                {
                    childMatch = &child;
                    break;
                }
            }

            if (childMatch == nullptr)
            {
                DirectoryNode newChild;
                newChild.RelativePath = accumulatedRel;
                newChild.DisplayName = segment;
                current->Children.push_back(std::move(newChild));
                childMatch = &current->Children.back();
            }

            current = childMatch;
        }

        return current;
    }

    void AssetTreeModel::InsertAssetSorted(std::vector<const AssetMeta*>& assets, const AssetMeta* meta)
    {
        if (meta == nullptr)
        {
            return;
        }

        for (const AssetMeta* existing : assets)
        {
            if (existing != nullptr && existing->AssetPath == meta->AssetPath)
            {
                return;
            }
        }

        auto insertPos = std::lower_bound(
            assets.begin(),
            assets.end(),
            meta,
            [](const AssetMeta* left, const AssetMeta* right)
            {
                return left->AssetName < right->AssetName;
            });
        assets.insert(insertPos, meta);
    }

    bool AssetTreeModel::RemoveAssetByPath(
        std::vector<const AssetMeta*>& assets,
        std::string_view assetPath)
    {
        const auto removeIter = std::find_if(
            assets.begin(),
            assets.end(),
            [assetPath](const AssetMeta* meta)
            {
                return meta != nullptr && meta->AssetPath == assetPath;
            });

        if (removeIter == assets.end())
        {
            return false;
        }

        assets.erase(removeIter);
        return true;
    }

    void AssetTreeModel::InsertIntoCurrentDirectoryList(const AssetMeta* meta)
    {
        if (meta == nullptr || !IsAssetInCurrentDirectory(meta->AssetPath))
        {
            return;
        }

        InsertAssetSorted(m_CurrentAssets, meta);
    }

    void AssetTreeModel::RemoveFromCurrentDirectoryList(std::string_view assetPath)
    {
        if (!IsAssetInCurrentDirectory(assetPath))
        {
            return;
        }

        RemoveAssetByPath(m_CurrentAssets, assetPath);
    }

    void AssetTreeModel::ApplyRegisteredChange(const AssetRegistryChange& change)
    {
        if (!AssetManager::HasInstance())
        {
            return;
        }

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(change.NewPath);
        if (meta == nullptr)
        {
            return;
        }

        const std::string parentRel = NormalizeParentDirectoryRel(change.NewPath);
        if (DirectoryNode* dirNode = GetOrInsertDirectoryNode(parentRel))
        {
            InsertAssetSorted(dirNode->Assets, meta);
        }

        InsertIntoCurrentDirectoryList(meta);
    }

    void AssetTreeModel::ApplyUnregisteredChange(const AssetRegistryChange& change)
    {
        const std::string parentRel = NormalizeParentDirectoryRel(change.OldPath);
        if (DirectoryNode* dirNode = FindDirectoryNode(parentRel))
        {
            RemoveAssetByPath(dirNode->Assets, change.OldPath);
        }

        RemoveFromCurrentDirectoryList(change.OldPath);
    }

    void AssetTreeModel::ApplyMovedChange(const AssetRegistryChange& change)
    {
        const std::string oldParentRel = NormalizeParentDirectoryRel(change.OldPath);
        if (DirectoryNode* oldDirNode = FindDirectoryNode(oldParentRel))
        {
            RemoveAssetByPath(oldDirNode->Assets, change.OldPath);
        }

        RemoveFromCurrentDirectoryList(change.OldPath);

        AssetRegistryChange registeredChange = change;
        registeredChange.Kind = AssetRegistryChangeKind::Registered;
        ApplyRegisteredChange(registeredChange);
    }

    void AssetTreeModel::ApplyMetaUpdatedChange(const AssetRegistryChange& change)
    {
        if (!IsAssetInCurrentDirectory(change.NewPath))
        {
            return;
        }

        if (!AssetManager::HasInstance())
        {
            return;
        }

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(change.NewPath);
        if (meta == nullptr)
        {
            return;
        }

        RemoveAssetByPath(m_CurrentAssets, change.NewPath);
        InsertAssetSorted(m_CurrentAssets, meta);
    }

    void AssetTreeModel::OnRegistryChange(const AssetRegistryChange& change)
    {
        if (m_ContentRoot.empty())
        {
            return;
        }

        switch (change.Kind)
        {
        case AssetRegistryChangeKind::Registered:
            ApplyRegisteredChange(change);
            break;
        case AssetRegistryChangeKind::Unregistered:
            ApplyUnregisteredChange(change);
            break;
        case AssetRegistryChangeKind::Moved:
            ApplyMovedChange(change);
            break;
        case AssetRegistryChangeKind::MetaUpdated:
        case AssetRegistryChangeKind::Reimported:
            ApplyMetaUpdatedChange(change);
            break;
        default:
            RebuildDirectoryTree();
            if (IsChangeRelevantToCurrentDirectory(change))
            {
                RebuildCurrentDirectoryAssetList();
            }
            break;
        }
    }

    void AssetTreeModel::RebuildDirectoryTree()
    {
        m_TreeRoot = DirectoryNode();
        if (m_ContentRoot.empty() || !std::filesystem::exists(m_ContentRoot))
        {
            return;
        }

        m_TreeRoot.RelativePath.clear();
        m_TreeRoot.DisplayName = m_ContentRoot.filename().string();
        if (m_TreeRoot.DisplayName.empty())
        {
            m_TreeRoot.DisplayName = "Assets";
        }

        BuildDirectoryNodeRecursive(m_TreeRoot, m_ContentRoot);
    }

    void AssetTreeModel::RebuildCurrentDirectoryAssetList()
    {
        m_CurrentAssets.clear();
        if (m_ContentRoot.empty() || !AssetManager::HasInstance())
        {
            return;
        }

        m_CurrentAssets = AssetManager::Get().FindAssetMetasUnderDirectory(m_CurrentDirectoryRel);
    }

    bool AssetTreeModel::IsAssetInCurrentDirectory(std::string_view assetRelativePath) const
    {
        const std::filesystem::path assetPath(assetRelativePath);
        const std::string parentRel = assetPath.parent_path().lexically_normal().generic_string();

        if (m_CurrentDirectoryRel.empty())
        {
            return parentRel.empty() || parentRel == ".";
        }

        return parentRel == m_CurrentDirectoryRel;
    }

    bool AssetTreeModel::IsChangeRelevantToCurrentDirectory(const AssetRegistryChange& change) const
    {
        switch (change.Kind)
        {
        case AssetRegistryChangeKind::Registered:
        case AssetRegistryChangeKind::MetaUpdated:
        case AssetRegistryChangeKind::Reimported:
            return IsAssetInCurrentDirectory(change.NewPath);
        case AssetRegistryChangeKind::Unregistered:
            return IsAssetInCurrentDirectory(change.OldPath);
        case AssetRegistryChangeKind::Moved:
            return IsAssetInCurrentDirectory(change.OldPath) || IsAssetInCurrentDirectory(change.NewPath);
        default:
            return true;
        }
    }

    void AssetTreeModel::BuildDirectoryNodeRecursive(
        DirectoryNode& outNode,
        const std::filesystem::path& absoluteDirectory)
    {
        outNode.Assets.clear();
        if (AssetManager::HasInstance())
        {
            outNode.Assets = AssetManager::Get().FindAssetMetasUnderDirectory(outNode.RelativePath);
        }

        std::error_code errorCode;
        if (!std::filesystem::is_directory(absoluteDirectory, errorCode))
        {
            return;
        }

        std::vector<std::filesystem::path> childDirectories;
        for (const auto& entry : std::filesystem::directory_iterator(absoluteDirectory, errorCode))
        {
            if (errorCode)
            {
                break;
            }

            if (!entry.is_directory(errorCode))
            {
                continue;
            }

            childDirectories.push_back(entry.path().lexically_normal());
        }

        std::sort(
            childDirectories.begin(),
            childDirectories.end(),
            [](const std::filesystem::path& left, const std::filesystem::path& right)
            {
                return left.filename().string() < right.filename().string();
            });

        outNode.Children.clear();
        outNode.Children.reserve(childDirectories.size());

        for (const std::filesystem::path& childDirectory : childDirectories)
        {
            DirectoryNode childNode;
            childNode.DisplayName = childDirectory.filename().string();

            std::error_code relativeError;
            childNode.RelativePath = std::filesystem::relative(
                                          childDirectory,
                                          m_ContentRoot,
                                          relativeError)
                                          .lexically_normal()
                                          .generic_string();
            if (relativeError)
            {
                continue;
            }

            BuildDirectoryNodeRecursive(childNode, childDirectory);
            outNode.Children.push_back(std::move(childNode));
        }
    }
}
