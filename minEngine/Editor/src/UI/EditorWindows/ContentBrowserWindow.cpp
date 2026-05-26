#include "UI/EditorWindows/ContentBrowserWindow.h"

#include "Services/AssetWorkflowModule.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
#include "Shell/IEditorContext.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Resource/AssetMeta.h"

#include "imgui.h"

#include <algorithm>
#include <filesystem>
#include <string>

namespace minEngine
{
    ContentBrowserWindow::ContentBrowserWindow(IEditorContext& context, AssetTreeModel& model)
        : EditorWindow(context)
        , m_Model(model)
    {
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
            m_Context.GetAssetWorkflow().ImportAssetDialog();
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

    void ContentBrowserWindow::DrawBreadcrumb()
    {
        if (ImGui::Button("Assets"))
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

            ImGui::SameLine();
            ImGui::TextUnformatted(">");
            ImGui::SameLine();

            ImGui::PushID(segmentIndex++);
            if (ImGui::Button(segment.c_str()))
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

        if (node.Children.empty() && node.Assets.empty())
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        const bool opened = ImGui::TreeNodeEx(node.DisplayName.c_str(), flags);
        if (ImGui::IsItemClicked())
        {
            m_Model.SetCurrentDirectory(node.RelativePath);
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

        ImGui::PopID();
    }

    void ContentBrowserWindow::DrawAssetTileGrid()
    {
        const std::vector<const AssetMeta*>& assets = m_Model.GetAssetsInCurrentDirectory();
        if (assets.empty())
        {
            ImGui::TextUnformatted("No registered assets in this folder.");
            return;
        }

        if (m_SelectedAssetIndex >= static_cast<int>(assets.size()))
        {
            m_SelectedAssetIndex = -1;
            m_Context.GetAssetWorkflow().SetSelectedAsset(nullptr);
        }

        const float tileWidth = 128.0f;
        const float tileHeight = 110.0f;
        const float spacing = 12.0f;
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const int columnCount = std::max(1, static_cast<int>((availableWidth + spacing) / (tileWidth + spacing)));

        for (int index = 0; index < static_cast<int>(assets.size()); ++index)
        {
            const AssetMeta* meta = assets[static_cast<size_t>(index)];
            if (meta == nullptr)
            {
                continue;
            }

            ImGui::PushID(index);
            ImGui::BeginGroup();

            const bool selectedTile = (m_SelectedAssetIndex == index);
            if (ImGui::Selectable("##tile", selectedTile, 0, ImVec2(tileWidth, tileHeight)))
            {
                SelectAsset(meta);
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                ActivateAssetFromBrowser(*meta);
            }

            const ImVec2 tileMin = ImGui::GetItemRectMin();
            const ImVec2 tileMax = ImGui::GetItemRectMax();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRect(
                ImVec2(tileMin.x + 8.0f, tileMin.y + 8.0f),
                ImVec2(tileMax.x - 8.0f, tileMin.y + 56.0f),
                IM_COL32(170, 170, 170, 255),
                4.0f);

            ImGui::SetCursorScreenPos(ImVec2(tileMin.x + 10.0f, tileMin.y + 64.0f));
            ImGui::PushTextWrapPos(tileMin.x + tileWidth - 10.0f);
            ImGui::TextUnformatted(meta->AssetName.c_str());
            ImGui::PopTextWrapPos();

            ImGui::EndGroup();
            ImGui::PopID();

            const bool endOfRow = ((index + 1) % columnCount) == 0;
            if (!endOfRow && index + 1 < static_cast<int>(assets.size()))
            {
                ImGui::SameLine(0.0f, spacing);
            }
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
