#include "UI/EditorWindows/ContentBrowserWindow.h"

#include "Services/AssetWorkflowModule.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
#include "Shell/IEditorContext.h"

#include "Runtime/Resource/AssetMeta.h"

#include "imgui.h"

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

        DrawToolbar();
        ImGui::Separator();

        const float treeWidth = ImGui::GetContentRegionAvail().x * 0.28f;
        ImGui::BeginChild("ContentBrowserTree", ImVec2(treeWidth, 0.0f), true);
        DrawDirectoryTree();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("ContentBrowserList", ImVec2(0.0f, 0.0f), true);
        DrawAssetList();
        DrawSelectionSummary();
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
        if (ImGui::Button("Delete"))
        {
            m_Context.GetAssetWorkflow().DeleteSelectedAsset();
            m_SelectedAssetIndex = -1;
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

        if (node.Children.empty())
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        const bool opened = ImGui::TreeNodeEx(node.DisplayName.c_str(), flags);
        if (ImGui::IsItemClicked())
        {
            m_Model.SetCurrentDirectory(node.RelativePath);
            m_SelectedAssetIndex = -1;
            m_Context.GetAssetWorkflow().SetSelectedAsset(nullptr);
        }

        if (opened && !node.Children.empty())
        {
            for (const AssetTreeModel::DirectoryNode& child : node.Children)
            {
                DrawDirectoryNode(child);
            }
            ImGui::TreePop();
        }
    }

    void ContentBrowserWindow::DrawAssetList()
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

        if (ImGui::BeginTable(
                "ContentBrowserAssetTable",
                3,
                ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Guid");
            ImGui::TableHeadersRow();

            for (int index = 0; index < static_cast<int>(assets.size()); ++index)
            {
                const AssetMeta* meta = assets[static_cast<size_t>(index)];
                if (meta == nullptr)
                {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::PushID(index);

                ImGui::TableNextColumn();
                const bool selected = (m_SelectedAssetIndex == index);
                if (ImGui::Selectable(meta->AssetName.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
                {
                    m_SelectedAssetIndex = index;
                    m_Context.GetAssetWorkflow().SetSelectedAsset(meta);
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    m_Context.GetAssetWorkflow().OpenAsset(*meta);
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(meta->AssetType.c_str());

                ImGui::TableNextColumn();
                const std::string guidShort = meta->Guid.ToString();
                ImGui::TextUnformatted(guidShort.c_str());

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

    void ContentBrowserWindow::DrawSelectionSummary()
    {
        const AssetMeta* selected = m_Context.GetAssetWorkflow().GetSelectedAsset();
        if (selected == nullptr)
        {
            return;
        }

        ImGui::Separator();
        ImGui::Text("Selected: %s (%s)", selected->AssetName.c_str(), selected->AssetType.c_str());
    }
}
