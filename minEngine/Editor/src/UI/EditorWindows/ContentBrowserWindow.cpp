#include "UI/EditorWindows/ContentBrowserWindow.h"

#include "ContextMenu/EditorContextMenuSystem.h"
#include "ContextMenu/EditorMenuContext.h"
#include "Services/AssetWorkflowModule.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
#include "Services/Inspector/InspectorModule.h"
#include "Shell/IEditorContext.h"

#include "Services/Thumbnail/AssetThumbnailService.h"
#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorTypographyDefaults.h"
#include "UI/Appearance/EditorTypographyScope.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"
#include "Runtime/Resource/AssetMeta.h"

#include "imgui.h"
#include "IconFontCppHeaders/IconsFontAwesome7.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>

namespace minEngine
{
    const char* ContentBrowserWindow::ResolveAssetTypeIconGlyph(const std::string_view assetType) const
    {
        if (assetType == "Texture2D")
        {
            return ICON_FA_IMAGE;
        }

        if (assetType == "StaticMesh")
        {
            return ICON_FA_CUBE;
        }

        if (assetType == "Material")
        {
            return ICON_FA_PALETTE;
        }

        if (assetType == "Scene")
        {
            return ICON_FA_MAP;
        }

        if (assetType == "Font")
        {
            return ICON_FA_FONT;
        }

        return ICON_FA_FILE;
    }

    AssetIconFontStyle ContentBrowserWindow::ResolveAssetTypeIconFontStyle(const std::string_view assetType) const
    {
        if (assetType == "StaticMesh" || assetType == "Material" || assetType == "Font")
        {
            return AssetIconFontStyle::Solid;
        }

        return AssetIconFontStyle::Regular;
    }

    void ContentBrowserWindow::DrawTileAssetIcon(
        const AssetMeta& meta,
        const ImVec2& iconMin,
        const ImVec2& iconMax,
        const EditorAppearance& appearance,
        const EditorThemePalette& palette,
        ImDrawList& drawList) const
    {
        drawList.AddRectFilled(
            iconMin,
            iconMax,
            appearance.GetDisplayColorU32(palette.FieldBackground),
            0.0f);
        drawList.AddRect(
            iconMin,
            iconMax,
            appearance.GetDisplayColorU32(palette.Border),
            0.0f,
            0,
            1.0f);

        if (meta.AssetType == "Texture2D" || meta.AssetType == "Material" || meta.AssetType == "StaticMesh")
        {
            AssetThumbnailService& thumbnails = m_Context.GetInspectorModule().GetThumbnailService();
            const ThumbnailView view = thumbnails.RequestThumbnailForAsset(meta);
            if (view.State == ThumbnailState::Ready && view.TextureId != 0 && view.Width > 0 && view.Height > 0)
            {
                const float slotWidth = iconMax.x - iconMin.x;
                const float slotHeight = iconMax.y - iconMin.y;

                const float textureAspect = static_cast<float>(view.Width) / static_cast<float>(view.Height);
                ImVec2 imageSize(slotWidth, slotHeight);
                if (textureAspect >= 1.0f)
                {
                    imageSize.y = slotHeight / textureAspect;
                }
                else
                {
                    imageSize.x = slotWidth * textureAspect;
                }

                const float imageX = iconMin.x + (slotWidth - imageSize.x) * 0.5f;
                const float imageY = iconMin.y + (slotHeight - imageSize.y) * 0.5f;
                const ImVec2 imageMin(imageX, imageY);
                const ImVec2 imageMax(imageX + imageSize.x, imageY + imageSize.y);

                // Flip V to match Inspector preview convention.
                drawList.AddImage(view.TextureId, imageMin, imageMax, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
                return;
            }
        }

        ImFont* iconFont = nullptr;
        const AssetIconFontStyle iconFontStyle = ResolveAssetTypeIconFontStyle(meta.AssetType);
        if (iconFontStyle == AssetIconFontStyle::Solid)
        {
            iconFont = appearance.GetAssetIconSolidImFont();
            if (iconFont == nullptr)
            {
                iconFont = appearance.GetAssetIconRegularImFont();
            }
        }
        else
        {
            iconFont = appearance.GetAssetIconRegularImFont();
            if (iconFont == nullptr)
            {
                iconFont = appearance.GetAssetIconSolidImFont();
            }
        }
        if (iconFont == nullptr)
        {
            return;
        }

        const char* glyph = ResolveAssetTypeIconGlyph(meta.AssetType);
        if (glyph == nullptr || glyph[0] == '\0')
        {
            return;
        }

        ImGui::PushFont(iconFont, 0.0f);
        const ImVec2 glyphSize = ImGui::CalcTextSize(glyph);
        const float glyphX = iconMin.x + (ViewMetrics::IconSize - glyphSize.x) * 0.5f;
        const float glyphY = iconMin.y + (ViewMetrics::IconSize - glyphSize.y) * 0.5f;
        drawList.AddText(ImVec2(glyphX, glyphY), appearance.GetDisplayColorU32(palette.TextPrimary), glyph);
        ImGui::PopFont();
    }

    ContentBrowserWindow::ContentBrowserWindow(IEditorContext& context, AssetTreeModel& model)
        : EditorWindow(context)
        , m_Model(model)
    {
    }

    float ContentBrowserWindow::ResolveTileOuterHeight() const
    {
        const float labelLineHeight =
            EditorTypographyDefaults::GetDefaultSizePixels(EditorTypographyRole::Caption);
        return ViewMetrics::TilePadding + ViewMetrics::IconSize + ViewMetrics::IconLabelGap + labelLineHeight +
               ViewMetrics::TilePadding;
    }

    std::string ContentBrowserWindow::BuildEllipsizedLabel(const char* text, const float maxWidth) const
    {
        if (text == nullptr || text[0] == '\0')
        {
            return {};
        }

        const ImVec2 fullSize = ImGui::CalcTextSize(text);
        if (fullSize.x <= maxWidth)
        {
            return text;
        }

        constexpr const char* kEllipsis = "...";
        const float ellipsisWidth = ImGui::CalcTextSize(kEllipsis).x;
        const float textBudget = maxWidth - ellipsisWidth;
        if (textBudget <= 0.0f)
        {
            return kEllipsis;
        }

        const size_t textLength = std::strlen(text);
        for (size_t fitLength = textLength; fitLength > 0; --fitLength)
        {
            const std::string trial(text, fitLength);
            if (ImGui::CalcTextSize(trial.c_str()).x <= textBudget)
            {
                return trial + kEllipsis;
            }
        }

        return kEllipsis;
    }

    void ContentBrowserWindow::OnDraw()
    {
        ImGui::Begin(m_Title.c_str());

        const bool browserFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_Context.GetAssetWorkflow().SetContentBrowserInspectorActive(browserFocused);
        SyncSelectionFromWorkflow();

        DrawToolbar();
        DrawBreadcrumb();
        ImGui::Separator();

        const float treeWidth = ImGui::GetContentRegionAvail().x * 0.28f;
        ImGui::BeginChild("ContentBrowserTree", ImVec2(treeWidth, 0.0f), true);
        DrawDirectoryTree();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("ContentBrowserList", ImVec2(0.0f, 0.0f), true);
        DrawAssetTileGrid();
        ImGui::EndChild();

        ImGui::End();
    }

    void ContentBrowserWindow::DrawToolbar()
    {
        if (ImGui::Button("Import"))
        {
            m_Context.GetAssetWorkflow().ImportAssetDialog(m_Model.GetCurrentDirectory());
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
        {
            m_Model.RebuildDirectoryTree();
            m_Model.RebuildCurrentDirectoryAssetList();
            m_SelectedAssetIndex = -1;
            m_Context.GetAssetWorkflow().SetSelectedAsset(nullptr);
        }
    }

    bool ContentBrowserWindow::DrawBreadcrumbLink(const char* label)
    {
        const EditorAppearance& appearance = m_Context.GetEditorAppearance();
        const EditorThemePalette& palette = appearance.GetActivePalette();

        const ImVec2 textSize = ImGui::CalcTextSize(label);
        const ImVec2 buttonSize(textSize.x + ViewMetrics::BreadcrumbPadX * 2.0f,
                                textSize.y + ViewMetrics::BreadcrumbPadY * 2.0f);

        const bool pressed = ImGui::InvisibleButton(label, buttonSize);
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (hovered)
        {
            drawList->AddRectFilled(
                itemMin,
                itemMax,
                appearance.GetDisplayColorU32(palette.Selection, 0.35f),
                0.0f);
        }

        const ImU32 textColor = appearance.GetDisplayColorU32(palette.TextPrimary);
        const ImVec2 textPos(itemMin.x + ViewMetrics::BreadcrumbPadX, itemMin.y + ViewMetrics::BreadcrumbPadY);
        drawList->AddText(textPos, textColor, label);

        return pressed;
    }

    void ContentBrowserWindow::DrawBreadcrumbSeparator()
    {
        ImGui::SameLine(0.0f, ViewMetrics::BreadcrumbSeparatorSpacing);
        const EditorAppearance& appearance = m_Context.GetEditorAppearance();
        ImGui::PushStyleColor(ImGuiCol_Text, appearance.GetDisplayColor(appearance.GetActivePalette().TextMuted));
        ImGui::TextUnformatted(">");
        ImGui::PopStyleColor();
    }

    void ContentBrowserWindow::DrawBreadcrumb()
    {
        EditorTypographyScope captionTypography(m_Context.GetEditorAppearance(), EditorTypographyRole::Caption);

        if (DrawBreadcrumbLink("Assets"))
        {
            m_Model.SetCurrentDirectory({});
        }

        const std::string currentDirectory(m_Model.GetCurrentDirectory());
        if (currentDirectory.empty())
        {
            return;
        }

        size_t segmentStart = 0;
        int segmentIndex = 0;
        while (segmentStart < currentDirectory.size())
        {
            const size_t separatorPos = currentDirectory.find('/', segmentStart);
            const size_t segmentEnd = separatorPos == std::string::npos ? currentDirectory.size() : separatorPos;
            const std::string segment = currentDirectory.substr(segmentStart, segmentEnd - segmentStart);
            const std::string prefix = currentDirectory.substr(0, segmentEnd);

            DrawBreadcrumbSeparator();
            ImGui::SameLine(0.0f, 0.0f);

            ImGui::PushID(segmentIndex++);
            if (DrawBreadcrumbLink(segment.c_str()))
            {
                m_Model.SetCurrentDirectory(prefix);
            }
            ImGui::PopID();

            if (separatorPos == std::string::npos)
            {
                break;
            }

            segmentStart = separatorPos + 1;
        }
    }

    void ContentBrowserWindow::DrawDirectoryTree()
    {
        const AssetTreeModel::DirectoryNode& root = m_Model.GetDirectoryTreeRoot();
        if (root.DisplayName.empty())
        {
            ImGui::TextUnformatted("No project content root.");
            return;
        }

        DrawDirectoryNode(root);
    }

    void ContentBrowserWindow::DrawDirectoryNode(const AssetTreeModel::DirectoryNode& node)
    {
        const bool isRoot = node.RelativePath.empty();
        const bool selected =
            isRoot ? m_Model.GetCurrentDirectory().empty() : m_Model.GetCurrentDirectory() == node.RelativePath;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (isRoot)
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        if (selected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const bool isLeaf = node.Children.empty() && node.Assets.empty();
        if (isLeaf)
        {
            // NoTreePushOnOpen: TreePop must not run when TreeNodeEx reports open (see imgui_demo.cpp ~9798).
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        bool opened = ImGui::TreeNodeEx(node.DisplayName.c_str(), flags);
        if (isLeaf)
        {
            opened = false;
        }

        if (ImGui::IsItemClicked())
        {
            m_Model.SetCurrentDirectory(node.RelativePath);
        }

        if (ImGui::BeginPopupContextItem())
        {
            DrawContentBrowserContextMenu(
                ContentBrowserHitKind::TreeDirectory,
                node.RelativePath,
                nullptr);
            ImGui::EndPopup();
        }

        if (opened)
        {
            for (const AssetTreeModel::DirectoryNode& child : node.Children)
            {
                DrawDirectoryNode(child);
            }

            for (const AssetMeta* assetMeta : node.Assets)
            {
                if (assetMeta == nullptr)
                {
                    continue;
                }

                DrawAssetTreeLeaf(*assetMeta);
            }

            ImGui::TreePop();
        }
    }

    void ContentBrowserWindow::DrawAssetTreeLeaf(const AssetMeta& assetMeta)
    {
        ImGui::PushID(assetMeta.AssetPath.c_str());
        const AssetMeta* selected = m_Context.GetAssetWorkflow().GetSelectedAsset();
        const bool isSelected = selected != nullptr && selected->AssetPath == assetMeta.AssetPath;
        if (ImGui::Selectable(assetMeta.AssetName.c_str(), isSelected))
        {
            const std::string parentDirectory = std::filesystem::path(assetMeta.AssetPath)
                                                    .parent_path()
                                                    .lexically_normal()
                                                    .generic_string();
            m_Model.SetCurrentDirectory(parentDirectory);
            SelectAsset(&assetMeta);
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            ActivateAssetFromBrowser(assetMeta);
        }

        if (ImGui::BeginPopupContextItem())
        {
            DrawContentBrowserContextMenu(
                ContentBrowserHitKind::TreeAsset,
                m_Model.GetCurrentDirectory(),
                &assetMeta);
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    void ContentBrowserWindow::AdvanceTileLayout(const int tileIndex, const int columnCount)
    {
        if (tileIndex <= 0)
        {
            return;
        }

        if ((tileIndex % columnCount) == 0)
        {
            ImGui::Dummy(ImVec2(0.0f, ViewMetrics::TileSpacing));
        }
        else
        {
            ImGui::SameLine(0.0f, ViewMetrics::TileSpacing);
        }
    }

    void ContentBrowserWindow::DrawTileVisual(const char* label, const bool selected, const AssetMeta* iconAssetMeta)
    {
        const EditorAppearance& appearance = m_Context.GetEditorAppearance();
        const EditorThemePalette& palette = appearance.GetActivePalette();
        ImFont* captionFont = appearance.GetImFont(EditorTypographyRole::Caption);

        const ImVec2 outerMin = ImGui::GetItemRectMin();
        const ImVec2 outerMax = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->PushClipRect(outerMin, outerMax, true);

        if (selected)
        {
            drawList->AddRectFilled(
                outerMin,
                outerMax,
                appearance.GetDisplayColorU32(palette.Selection, 0.22f),
                0.0f);
            drawList->AddRect(
                outerMin,
                outerMax,
                appearance.GetDisplayColorU32(palette.Selection),
                0.0f,
                0,
                2.0f);
        }

        const float iconLeft = outerMin.x + (ViewMetrics::TileOuterWidth - ViewMetrics::IconSize) * 0.5f;
        const float iconTop = outerMin.y + ViewMetrics::TilePadding;
        const ImVec2 iconMin(iconLeft, iconTop);
        const ImVec2 iconMax(iconLeft + ViewMetrics::IconSize, iconTop + ViewMetrics::IconSize);

        if (iconAssetMeta != nullptr)
        {
            DrawTileAssetIcon(*iconAssetMeta, iconMin, iconMax, appearance, palette, *drawList);
        }
        else
        {
            drawList->AddRectFilled(
                iconMin,
                iconMax,
                appearance.GetDisplayColorU32(palette.FieldBackground),
                0.0f);
            drawList->AddRect(
                iconMin,
                iconMax,
                appearance.GetDisplayColorU32(palette.Border),
                0.0f,
                0,
                1.0f);
        }

        const float labelTop = iconMax.y + ViewMetrics::IconLabelGap;
        const ImVec2 labelMin(outerMin.x + ViewMetrics::TilePadding, labelTop);
        const ImVec2 labelMax(outerMax.x - ViewMetrics::TilePadding, outerMax.y - ViewMetrics::TilePadding);
        const float labelWidth = labelMax.x - labelMin.x;

        const ImU32 labelColor = appearance.GetDisplayColorU32(palette.TextPrimary);

        if (captionFont != nullptr)
        {
            ImGui::PushFont(captionFont, 0.0f);
        }

        const std::string displayName = BuildEllipsizedLabel(label, labelWidth);
        const ImVec2 displaySize = ImGui::CalcTextSize(displayName.c_str());
        const float textX = labelMin.x + (labelWidth - displaySize.x) * 0.5f;
        drawList->AddText(ImVec2(textX, labelMin.y), labelColor, displayName.c_str());

        if (captionFont != nullptr)
        {
            ImGui::PopFont();
        }

        drawList->PopClipRect();
    }

    void ContentBrowserWindow::DrawDirectoryTile(const AssetTreeModel::DirectoryNode& directoryNode)
    {
        ImGui::PushID(directoryNode.RelativePath.c_str());

        const ImVec2 outerSize(ViewMetrics::TileOuterWidth, ResolveTileOuterHeight());
        ImGui::InvisibleButton("##dirTile", outerSize);

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            m_Model.SetCurrentDirectory(directoryNode.RelativePath);
            m_SelectedAssetIndex = -1;
            m_Context.GetAssetWorkflow().SetSelectedAsset(nullptr);
        }

        DrawTileVisual(directoryNode.DisplayName.c_str(), false);

        if (ImGui::BeginPopupContextItem())
        {
            DrawContentBrowserContextMenu(
                ContentBrowserHitKind::TreeDirectory,
                directoryNode.RelativePath,
                nullptr);
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    void ContentBrowserWindow::DrawAssetTile(const AssetMeta& meta, const int tileIndex, const bool selected)
    {
        ImGui::PushID(tileIndex);

        const ImVec2 outerSize(ViewMetrics::TileOuterWidth, ResolveTileOuterHeight());
        if (ImGui::InvisibleButton("##tile", outerSize))
        {
            SelectAsset(&meta);
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            ActivateAssetFromBrowser(meta);
        }

        DrawTileVisual(meta.AssetName.c_str(), selected, &meta);

        if (ImGui::BeginPopupContextItem())
        {
            DrawContentBrowserContextMenu(
                ContentBrowserHitKind::TileAsset,
                m_Model.GetCurrentDirectory(),
                &meta);
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    void ContentBrowserWindow::DrawAssetTileGrid()
    {
        if (ImGui::BeginPopupContextWindow(
                "ContentBrowserListBackground",
                ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            DrawContentBrowserContextMenu(
                ContentBrowserHitKind::ListBackground,
                m_Model.GetCurrentDirectory(),
                nullptr);
            ImGui::EndPopup();
        }

        const std::vector<const AssetTreeModel::DirectoryNode*>& subdirectories =
            m_Model.GetSubdirectoriesInCurrentDirectory();
        const std::vector<const AssetMeta*>& assets = m_Model.GetAssetsInCurrentDirectory();
        if (subdirectories.empty() && assets.empty())
        {
            ImGui::TextUnformatted("No folders or registered assets in this folder.");
            return;
        }

        if (m_SelectedAssetIndex >= static_cast<int>(assets.size()))
        {
            m_SelectedAssetIndex = -1;
            m_Context.GetAssetWorkflow().SetSelectedAsset(nullptr);
        }

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const int columnCount = std::max(
            1,
            static_cast<int>((availableWidth + ViewMetrics::TileSpacing) /
                             (ViewMetrics::TileOuterWidth + ViewMetrics::TileSpacing)));

        int tileIndex = 0;
        for (const AssetTreeModel::DirectoryNode* directoryNode : subdirectories)
        {
            if (directoryNode == nullptr)
            {
                continue;
            }

            AdvanceTileLayout(tileIndex, columnCount);
            DrawDirectoryTile(*directoryNode);
            ++tileIndex;
        }

        for (int index = 0; index < static_cast<int>(assets.size()); ++index)
        {
            const AssetMeta* meta = assets[static_cast<size_t>(index)];
            if (meta == nullptr)
            {
                continue;
            }

            AdvanceTileLayout(tileIndex, columnCount);
            const bool selectedTile = (m_SelectedAssetIndex == index);
            DrawAssetTile(*meta, tileIndex, selectedTile);
            ++tileIndex;
        }
    }

    void ContentBrowserWindow::ActivateAssetFromBrowser(const AssetMeta& assetMeta)
    {
        ME_CORE_INFO(
            "ContentBrowser: activate asset '{}' (type: '{}') - editor routing is deferred.",
            assetMeta.AssetPath,
            assetMeta.AssetType);
    }

    void ContentBrowserWindow::SelectAsset(const AssetMeta* meta)
    {
        m_Context.GetAssetWorkflow().SetSelectedAsset(meta);
        SyncSelectionFromWorkflow();
    }

    void ContentBrowserWindow::DrawContentBrowserContextMenu(
        const ContentBrowserHitKind hitKind,
        const std::string_view directoryRel,
        const AssetMeta* assetForContext)
    {
        if (assetForContext != nullptr)
        {
            const AssetMeta* selected = m_Context.GetAssetWorkflow().GetSelectedAsset();
            if (selected == nullptr || selected->AssetPath != assetForContext->AssetPath)
            {
                SelectAsset(assetForContext);
            }
        }

        auto cbContext = std::make_shared<ContentBrowserMenuContext>();
        cbContext->HitKind = hitKind;
        cbContext->CurrentDirectoryRel = std::string(directoryRel);

        if (assetForContext != nullptr)
        {
            cbContext->SelectedAssets.push_back(assetForContext);
        }
        else
        {
            const AssetMeta* selected = m_Context.GetAssetWorkflow().GetSelectedAsset();
            if (selected != nullptr)
            {
                cbContext->SelectedAssets.push_back(selected);
            }
        }

        EditorMenuContext menuContext;
        menuContext.Add(cbContext);
        m_Context.GetContextMenu().BuildAndDraw(m_Context, menuContext);
    }

    void ContentBrowserWindow::SyncSelectionFromWorkflow()
    {
        const AssetMeta* selected = m_Context.GetAssetWorkflow().GetSelectedAsset();
        if (selected == nullptr)
        {
            m_SelectedAssetIndex = -1;
            return;
        }

        const std::vector<const AssetMeta*>& assets = m_Model.GetAssetsInCurrentDirectory();
        m_SelectedAssetIndex = -1;
        for (int index = 0; index < static_cast<int>(assets.size()); ++index)
        {
            const AssetMeta* assetMeta = assets[static_cast<size_t>(index)];
            if (assetMeta != nullptr && assetMeta->AssetPath == selected->AssetPath)
            {
                m_SelectedAssetIndex = index;
                return;
            }
        }
    }
}
