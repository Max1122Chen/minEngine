#pragma once

#include "ContextMenu/Contexts/ContentBrowserMenuContext.h"
#include "Core.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
#include "UI/EditorWindows/EditorWindow.h"

#include <string>

struct ImDrawList;
struct ImVec2;

namespace minEngine
{
    class EditorAppearance;
    struct EditorThemePalette;
    enum class AssetIconFontStyle
    {
        Regular,
        Solid
    };

    class ContentBrowserWindow final : public EditorWindow
    {
    public:
        ContentBrowserWindow(IEditorContext& context, AssetTreeModel& model);

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }
        std::string_view GetOwnerModuleId() const override { return "Scene"; }

        void OnDraw() override;

    private:
        struct ViewMetrics
        {
            static constexpr float TileOuterWidth = 128.0f;
            static constexpr float TilePadding = 4.0f;
            static constexpr float IconSize = TileOuterWidth - 2.0f * TilePadding;
            static constexpr float IconLabelGap = 4.0f;
            static constexpr float TileSpacing = 8.0f;
            static constexpr float BreadcrumbPadX = 6.0f;
            static constexpr float BreadcrumbPadY = 2.0f;
            static constexpr float BreadcrumbSeparatorSpacing = 4.0f;
        };

        void DrawToolbar();
        void DrawBreadcrumb();
        bool DrawBreadcrumbLink(const char* label);
        void DrawBreadcrumbSeparator();
        void DrawDirectoryTree();
        void DrawDirectoryNode(const AssetTreeModel::DirectoryNode& node);
        void DrawAssetTileGrid();
        void AdvanceTileLayout(int tileIndex, int columnCount);
        void DrawTileVisual(const char* label, bool selected, const AssetMeta* iconAssetMeta = nullptr);
        void DrawDirectoryTile(const AssetTreeModel::DirectoryNode& directoryNode);
        void DrawAssetTile(const AssetMeta& meta, int tileIndex, bool selected);
        void DrawTileAssetIcon(const AssetMeta& meta,
                               const ImVec2& iconMin,
                               const ImVec2& iconMax,
                               const EditorAppearance& appearance,
                               const EditorThemePalette& palette,
                               ImDrawList& drawList) const;
        void DrawAssetTreeLeaf(const AssetMeta& assetMeta);
        void ActivateAssetFromBrowser(const AssetMeta& assetMeta);
        void SelectAsset(const AssetMeta* meta);
        void SyncSelectionFromWorkflow();
        void DrawContentBrowserContextMenu(ContentBrowserHitKind hitKind,
                                           std::string_view directoryRel,
                                           const AssetMeta* assetForContext);

        float ResolveTileOuterHeight() const;
        std::string BuildEllipsizedLabel(const char* text, float maxWidth) const;
        const char* ResolveAssetTypeIconGlyph(std::string_view assetType) const;
        AssetIconFontStyle ResolveAssetTypeIconFontStyle(std::string_view assetType) const;

        AssetTreeModel& m_Model;
        const std::string m_Id = "ContentBrowser";
        const std::string m_Title = "Content Browser";
        int m_SelectedAssetIndex = -1;
    };
}
