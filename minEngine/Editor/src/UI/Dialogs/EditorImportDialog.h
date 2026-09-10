#pragma once

#include "Core.h"

#include <filesystem>
#include <string>
#include <vector>

namespace minEngine
{
    enum class EditorImportDialogAction
    {
        None,
        Confirm,
        Cancel
    };

    // Shared import product picker driven by AssetManager::GetImportProducts().
    class EditorImportDialog
    {
    public:
        void Open(
            std::vector<std::filesystem::path> sourcePaths,
            std::filesystem::path destDirectory);
        EditorImportDialogAction Draw();
        bool IsOpen() const { return m_Open; }
        void Close();

        const std::vector<std::filesystem::path>& GetSourcePaths() const { return m_SourcePaths; }
        const std::filesystem::path& GetDestDirectory() const { return m_DestDirectory; }
        const std::string& GetSelectedProductId() const { return m_SelectedProductId; }
        const std::string& GetSkeletonAssetPath() const { return m_SkeletonAssetPath; }

    private:
        void RebuildCompatibleProducts();
        bool SelectedProductNeedsSkeletonPicker() const;

        bool m_Open = false;
        std::vector<std::filesystem::path> m_SourcePaths;
        std::filesystem::path m_DestDirectory;
        std::vector<std::string> m_CompatibleProductIds;
        std::vector<std::string> m_CompatibleProductNames;
        int m_SelectedProductIndex = 0;
        std::string m_SelectedProductId;
        std::string m_SkeletonAssetPath;
    };
}
