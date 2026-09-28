#include "Services/ContentBrowser/ContentBrowserSelection.h"

#include <algorithm>

namespace minEngine
{
    void ContentBrowserSelection::Clear()
    {
        m_Entries.clear();
        m_PrimaryIndex = -1;
    }

    const ContentBrowserSelectionEntry* ContentBrowserSelection::GetPrimary() const
    {
        if (m_PrimaryIndex < 0 || m_PrimaryIndex >= static_cast<int>(m_Entries.size()))
        {
            return nullptr;
        }
        return &m_Entries[static_cast<size_t>(m_PrimaryIndex)];
    }

    std::string ContentBrowserSelection::GetPrimaryAssetPath() const
    {
        const ContentBrowserSelectionEntry* primary = GetPrimary();
        if (primary == nullptr || primary->Kind != ContentBrowserSelectionKind::Asset)
        {
            return {};
        }
        return primary->Path;
    }

    std::vector<std::string> ContentBrowserSelection::GetSelectedAssetPaths() const
    {
        std::vector<std::string> paths;
        paths.reserve(m_Entries.size());
        for (const ContentBrowserSelectionEntry& entry : m_Entries)
        {
            if (entry.Kind == ContentBrowserSelectionKind::Asset)
            {
                paths.push_back(entry.Path);
            }
        }
        return paths;
    }

    std::vector<std::string> ContentBrowserSelection::GetSelectedDirectoryPaths() const
    {
        std::vector<std::string> paths;
        for (const ContentBrowserSelectionEntry& entry : m_Entries)
        {
            if (entry.Kind == ContentBrowserSelectionKind::Directory)
            {
                paths.push_back(entry.Path);
            }
        }
        return paths;
    }

    bool ContentBrowserSelection::HasMixedKinds() const
    {
        bool hasAsset = false;
        bool hasDirectory = false;
        for (const ContentBrowserSelectionEntry& entry : m_Entries)
        {
            if (entry.Kind == ContentBrowserSelectionKind::Asset)
            {
                hasAsset = true;
            }
            else
            {
                hasDirectory = true;
            }
            if (hasAsset && hasDirectory)
            {
                return true;
            }
        }
        return false;
    }

    bool ContentBrowserSelection::AllAssets() const
    {
        return !m_Entries.empty() && !HasMixedKinds()
            && m_Entries.front().Kind == ContentBrowserSelectionKind::Asset;
    }

    bool ContentBrowserSelection::AllDirectories() const
    {
        return !m_Entries.empty() && !HasMixedKinds()
            && m_Entries.front().Kind == ContentBrowserSelectionKind::Directory;
    }

    int ContentBrowserSelection::FindIndex(ContentBrowserSelectionKind kind, std::string_view path) const
    {
        for (int index = 0; index < static_cast<int>(m_Entries.size()); ++index)
        {
            const ContentBrowserSelectionEntry& entry = m_Entries[static_cast<size_t>(index)];
            if (entry.Kind == kind && entry.Path == path)
            {
                return index;
            }
        }
        return -1;
    }

    void ContentBrowserSelection::SetPrimaryIndex(int index)
    {
        m_PrimaryIndex = index;
    }

    void ContentBrowserSelection::SetSingleAsset(std::string_view assetPath)
    {
        m_Entries.clear();
        if (assetPath.empty())
        {
            m_PrimaryIndex = -1;
            return;
        }
        ContentBrowserSelectionEntry entry;
        entry.Kind = ContentBrowserSelectionKind::Asset;
        entry.Path = std::string(assetPath);
        m_Entries.push_back(std::move(entry));
        m_PrimaryIndex = 0;
    }

    void ContentBrowserSelection::SetSingleDirectory(std::string_view directoryRel)
    {
        m_Entries.clear();
        ContentBrowserSelectionEntry entry;
        entry.Kind = ContentBrowserSelectionKind::Directory;
        entry.Path = std::string(directoryRel);
        m_Entries.push_back(std::move(entry));
        m_PrimaryIndex = 0;
    }

    void ContentBrowserSelection::ToggleAsset(std::string_view assetPath)
    {
        if (assetPath.empty())
        {
            return;
        }
        const int existing = FindIndex(ContentBrowserSelectionKind::Asset, assetPath);
        if (existing >= 0)
        {
            m_Entries.erase(m_Entries.begin() + existing);
            if (m_Entries.empty())
            {
                m_PrimaryIndex = -1;
            }
            else if (m_PrimaryIndex >= static_cast<int>(m_Entries.size()))
            {
                m_PrimaryIndex = static_cast<int>(m_Entries.size()) - 1;
            }
            else if (existing < m_PrimaryIndex)
            {
                --m_PrimaryIndex;
            }
            else if (existing == m_PrimaryIndex)
            {
                m_PrimaryIndex = static_cast<int>(m_Entries.size()) - 1;
            }
            return;
        }

        ContentBrowserSelectionEntry entry;
        entry.Kind = ContentBrowserSelectionKind::Asset;
        entry.Path = std::string(assetPath);
        m_Entries.push_back(std::move(entry));
        m_PrimaryIndex = static_cast<int>(m_Entries.size()) - 1;
    }

    void ContentBrowserSelection::ToggleDirectory(std::string_view directoryRel)
    {
        const int existing = FindIndex(ContentBrowserSelectionKind::Directory, directoryRel);
        if (existing >= 0)
        {
            m_Entries.erase(m_Entries.begin() + existing);
            if (m_Entries.empty())
            {
                m_PrimaryIndex = -1;
            }
            else if (m_PrimaryIndex >= static_cast<int>(m_Entries.size()))
            {
                m_PrimaryIndex = static_cast<int>(m_Entries.size()) - 1;
            }
            else if (existing < m_PrimaryIndex)
            {
                --m_PrimaryIndex;
            }
            else if (existing == m_PrimaryIndex)
            {
                m_PrimaryIndex = static_cast<int>(m_Entries.size()) - 1;
            }
            return;
        }

        ContentBrowserSelectionEntry entry;
        entry.Kind = ContentBrowserSelectionKind::Directory;
        entry.Path = std::string(directoryRel);
        m_Entries.push_back(std::move(entry));
        m_PrimaryIndex = static_cast<int>(m_Entries.size()) - 1;
    }

    void ContentBrowserSelection::SetAssetRange(
        const std::vector<std::string>& orderedPathsInView,
        int fromIndex,
        int toIndex)
    {
        if (orderedPathsInView.empty())
        {
            Clear();
            return;
        }

        const int clampedFrom = std::clamp(fromIndex, 0, static_cast<int>(orderedPathsInView.size()) - 1);
        const int clampedTo = std::clamp(toIndex, 0, static_cast<int>(orderedPathsInView.size()) - 1);
        const int begin = std::min(clampedFrom, clampedTo);
        const int end = std::max(clampedFrom, clampedTo);

        m_Entries.clear();
        for (int index = begin; index <= end; ++index)
        {
            ContentBrowserSelectionEntry entry;
            entry.Kind = ContentBrowserSelectionKind::Asset;
            entry.Path = orderedPathsInView[static_cast<size_t>(index)];
            m_Entries.push_back(std::move(entry));
        }
        m_PrimaryIndex = static_cast<int>(m_Entries.size()) - 1;
    }
}
