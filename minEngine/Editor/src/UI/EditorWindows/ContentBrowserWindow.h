#pragma once

#include "Core.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
#include "UI/EditorWindows/EditorWindow.h"

namespace minEngine
{

    class ContentBrowserWindow final : public EditorWindow
    {
    public:
        ContentBrowserWindow(IEditorContext& context, AssetTreeModel& model);

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }
        std::string_view GetOwnerModuleId() const override { return "Scene"; }

        void OnDraw() override;

    private:
        void DrawToolbar();
        void DrawBreadcrumb();
        void DrawDirectoryTree();
        void DrawDirectoryNode(const AssetTreeModel::DirectoryNode& node);
        void DrawAssetTileGrid();
        void DrawAssetTreeLeaf(const AssetMeta& assetMeta);
        void ActivateAssetFromBrowser(const AssetMeta& assetMeta);
        void SelectAsset(const AssetMeta* meta);
        void SyncSelectionFromWorkflow();

        AssetTreeModel& m_Model;
        const std::string m_Id = "ContentBrowser";
        const std::string m_Title = "Content Browser";
        int m_SelectedAssetIndex = -1;
    };
}
