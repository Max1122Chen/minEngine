#include "AnimGraphWindow.h"

#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "imgui.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

#include "SubEditor/AnimationGraph/AnimationGraphEditor.h"

#include <vector>

namespace minEngine
{
    AnimGraphWindow::AnimGraphWindow(IEditorContext& context)
        : EditorWindow(context)
    {
        SetOpen(false);
    }

    std::string_view AnimGraphWindow::GetOwnerModuleId() const
    {
        return AnimationGraphEditor::kModuleId;
    }

    void AnimGraphWindow::OnDraw()
    {
        if (!EditorWindowTypography::BeginPanel(
                m_Context,
                m_Title.c_str(),
                nullptr,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            return;
        }

        EditorTypographyScope bodyTypography(m_Context.GetEditorAppearance(), EditorTypographyRole::Body);

        AnimationGraphEditor* animGraphEditor = dynamic_cast<AnimationGraphEditor*>(
            m_Context.FindSubModule(AnimationGraphEditor::kModuleId));
        if (!animGraphEditor)
        {
            ImGui::End();
            return;
        }

        bool rebindGraph = false;
        if (animGraphEditor->ConsumeGraphCanvasInvalidation(rebindGraph))
        {
            if (rebindGraph)
            {
                animGraphEditor->ClearSelection();
                m_BoundGraph = nullptr;
                m_SmGraphDocument.Clear();
            }
            m_SmGraphWidget.ResetInteraction();
        }

        DrawToolbar();
        ImGui::Separator();

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::BeginChild("AnimGraphCanvas", avail, true, ImGuiWindowFlags_NoScrollbar);
        DrawCanvas();
        ImGui::EndChild();

        ImGui::End();
    }

    void AnimGraphWindow::DrawToolbar()
    {
        AnimationGraphEditor* animGraphEditor = dynamic_cast<AnimationGraphEditor*>(
            m_Context.FindSubModule(AnimationGraphEditor::kModuleId));
        if (!animGraphEditor)
        {
            return;
        }

        const AnimationGraphEditorSession& session = animGraphEditor->GetSession();
        const auto& graphMetas = animGraphEditor->GetGraphMetas();
        const int selectedIndex = animGraphEditor->GetSelectedGraphIndex();

        const char* previewLabel = "No AnimationGraph";
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(graphMetas.size()))
        {
            previewLabel = graphMetas[static_cast<size_t>(selectedIndex)]->AssetName.c_str();
        }
        else if (session.HasOpenGraph())
        {
            previewLabel = session.AssetPath.c_str();
        }

        ImGui::SetNextItemWidth(280.0f);
        if (graphMetas.empty())
        {
            ImGui::TextDisabled("No AnimationGraph assets found (open a project with Assets scanned).");
        }
        else if (ImGui::BeginCombo("##AnimGraphAssetCombo", previewLabel))
        {
            for (int i = 0; i < static_cast<int>(graphMetas.size()); ++i)
            {
                const AssetMeta* meta = graphMetas[static_cast<size_t>(i)];
                ImGui::PushID(i);
                const bool selected = (i == selectedIndex);
                if (ImGui::Selectable(meta->AssetName.c_str(), selected))
                {
                    animGraphEditor->OpenSession(meta);
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (!session.HasOpenGraph())
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Validate"))
        {
            std::string error;
            if (animGraphEditor->ValidateActiveGraph(&error))
            {
                ME_CORE_INFO("AnimationGraph Validate OK: '{}'", session.AssetPath);
            }
            else
            {
                ME_CORE_WARN("AnimationGraph Validate failed: {}", error);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(session.Dirty ? "Save *" : "Save"))
        {
            animGraphEditor->SaveActiveGraph();
        }

        ImGui::SameLine();
        if (ImGui::Button("Add State"))
        {
            animGraphEditor->AddStateAt(40.0f, 40.0f);
        }

        if (!session.HasOpenGraph())
        {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("New Graph"))
        {
            std::shared_ptr<AnimationGraph> created =
                AssetManager::Get().CreateAsset<AnimationGraph>("NewAnimGraph", "Assets/Animations");
            if (created)
            {
                animGraphEditor->RefreshGraphList();
                if (const AssetMeta* meta =
                        AssetManager::Get().FindAssetMetaByGuid(created->GetGuid()))
                {
                    animGraphEditor->OpenSession(meta);
                }
                else
                {
                    ME_CORE_WARN("AnimGraphWindow: created graph has no registry meta.");
                }
            }
        }
    }

    void AnimGraphWindow::DrawCanvas()
    {
        AnimationGraphEditor* animGraphEditor = dynamic_cast<AnimationGraphEditor*>(
            m_Context.FindSubModule(AnimationGraphEditor::kModuleId));
        if (!animGraphEditor)
        {
            return;
        }

        const AnimationGraphEditorSession& session = animGraphEditor->GetSession();
        if (!session.HasOpenGraph() || !session.GraphAsset)
        {
            ImGui::TextUnformatted("Open an AnimationGraph asset to edit its state machine.");
            return;
        }

        AnimationGraph& graph = *session.GraphAsset;
        if (m_BoundGraph != &graph)
        {
            animGraphEditor->ClearSelection();
            m_BoundGraph = &graph;
            m_SmGraphWidget.ResetInteraction();
            m_SmGraphDocument.Clear();
        }

        if (AnimGraphSmBridge::LayoutIfNeeded(graph.GetStateMachine()))
        {
            animGraphEditor->NotifyGraphChanged();
        }

        AnimGraphSmBridge::PullDocument(graph, m_SmGraphDocument);
        AnimGraphSmBridge::ApplyEditorTheme(m_Context.GetEditorAppearance(), m_SmGraphWidget.GetStyle());

        std::vector<SmGraph::EditEvent> events;
        m_SmGraphWidget.Draw("AnimSmGraph", m_SmGraphDocument, events);
        AnimGraphSmBridge::ApplyEditEvents(*animGraphEditor, graph, m_SmGraphDocument, events);
    }
}
