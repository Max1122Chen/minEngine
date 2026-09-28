#pragma once

#include "Core.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    enum class ContentBrowserSelectionKind
    {
        Asset,
        Directory
    };

    struct ContentBrowserSelectionEntry
    {
        ContentBrowserSelectionKind Kind = ContentBrowserSelectionKind::Asset;
        std::string Path;
    };

    /**
     * Ordered Content Browser selection. Single-select is size==1.
     * Primary is last-clicked (Inspector / default single-target ops).
     * Paths are project-relative; do not hold AssetMeta*.
     */
    class ContentBrowserSelection
    {
    public:
        void Clear();
        bool Empty() const { return m_Entries.empty(); }
        std::size_t Size() const { return m_Entries.size(); }

        const std::vector<ContentBrowserSelectionEntry>& GetEntries() const { return m_Entries; }
        int GetPrimaryIndex() const { return m_PrimaryIndex; }

        const ContentBrowserSelectionEntry* GetPrimary() const;
        std::string GetPrimaryAssetPath() const;
        std::vector<std::string> GetSelectedAssetPaths() const;
        std::vector<std::string> GetSelectedDirectoryPaths() const;
        bool HasMixedKinds() const;
        bool AllAssets() const;
        bool AllDirectories() const;

        void SetSingleAsset(std::string_view assetPath);
        void SetSingleDirectory(std::string_view directoryRel);
        void ToggleAsset(std::string_view assetPath);
        void ToggleDirectory(std::string_view directoryRel);
        void SetAssetRange(const std::vector<std::string>& orderedPathsInView, int fromIndex, int toIndex);

    private:
        int FindIndex(ContentBrowserSelectionKind kind, std::string_view path) const;
        void SetPrimaryIndex(int index);

        std::vector<ContentBrowserSelectionEntry> m_Entries;
        int m_PrimaryIndex = -1;
    };
}
