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

        void OnDraw() override;

    private:
        void DrawToolbar();
        void DrawDirectoryTree();
        void DrawDirectoryNode(const AssetTreeModel::DirectoryNode& node);
        void DrawAssetList();
        void DrawSelectionSummary();

        AssetTreeModel& m_Model;
        const std::string m_Id = "ContentBrowser";
        const std::string m_Title = "Content Browser";
        int m_SelectedAssetIndex = -1;
    };
}
