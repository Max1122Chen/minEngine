#include "AnimGraphWindow.h"

#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorThemeScope.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTheme.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "imgui.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

#include "SubEditor/AnimationGraph/AnimationGraphEditor.h"
#include "SubEditor/AnimationGraph/AnimGraphIds.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace minEngine
{
    namespace Ed = ax::NodeEditor;

    AnimGraphWindow::AnimGraphWindow(IEditorContext& context)
        : EditorWindow(context)
    {
        SetOpen(false);
    }

    AnimGraphWindow::~AnimGraphWindow()
    {
        ShutdownNodeEditor();
    }

    std::string_view AnimGraphWindow::GetOwnerModuleId() const
    {
        return AnimationGraphEditor::kModuleId;
    }

    void AnimGraphWindow::ShutdownNodeEditor()
    {
        if (m_NodeEditorContext)
        {
            Ed::DestroyEditor(m_NodeEditorContext);
            m_NodeEditorContext = nullptr;
        }
        m_BoundGraph = nullptr;
    }

    void AnimGraphWindow::EnsureNodeEditor()
    {
        if (m_NodeEditorContext)
        {
            return;
        }

        Ed::Config config;
        config.SettingsFile = nullptr;
        config.SaveSettings = nullptr;
        config.LoadSettings = nullptr;
        config.SaveNodeSettings = nullptr;
        config.LoadNodeSettings = nullptr;
        m_NodeEditorContext = Ed::CreateEditor(&config);
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
            }
            m_PushStoredPositionsToEditor = true;
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

        EnsureNodeEditor();
        DrawNodeEditor(*session.GraphAsset);
    }

    void AnimGraphWindow::DrawNodeEditor(AnimationGraph& graph)
    {
        Ed::SetCurrentEditor(m_NodeEditorContext);

        if (m_BoundGraph != &graph)
        {
            if (AnimationGraphEditor* editor = dynamic_cast<AnimationGraphEditor*>(
                    m_Context.FindSubModule(AnimationGraphEditor::kModuleId)))
            {
                editor->ClearSelection();
            }
            m_BoundGraph = &graph;
            m_PushStoredPositionsToEditor = true;
        }

        Ed::Begin("AnimGraphEditor", ImVec2(0.0f, 0.0f));

        if (m_PushStoredPositionsToEditor)
        {
            PushStoredPositionsToEditor(graph.GetStateMachine());
            m_PushStoredPositionsToEditor = false;
        }

        DrawNodes(graph.GetStateMachine());
        DrawLinks(graph.GetStateMachine());
        HandleCreateLink(graph);
        HandleDelete(graph);
        SyncSelectionFromEditor(graph.GetStateMachine());
        SyncNodePositions(graph.GetStateMachine());
        DrawBackgroundContextMenu(graph);

        Ed::End();
        Ed::SetCurrentEditor(nullptr);
    }

    void AnimGraphWindow::LayoutNodesIfNeeded(AnimStateMachine& stateMachine)
    {
        if (stateMachine.States.empty())
        {
            return;
        }

        bool needsLayout = true;
        for (const AnimState& state : stateMachine.States)
        {
            if (state.EditorPosX != 0.0f || state.EditorPosY != 0.0f)
            {
                needsLayout = false;
                break;
            }
        }

        if (!needsLayout)
        {
            return;
        }

        constexpr float kSpacingX = 260.0f;
        constexpr float kSpacingY = 140.0f;
        constexpr int kColumns = 3;

        for (size_t i = 0; i < stateMachine.States.size(); ++i)
        {
            const int column = static_cast<int>(i) % kColumns;
            const int row = static_cast<int>(i) / kColumns;
            stateMachine.States[i].EditorPosX = static_cast<float>(column) * kSpacingX;
            stateMachine.States[i].EditorPosY = static_cast<float>(row) * kSpacingY;
        }

        if (AnimationGraphEditor* editor = dynamic_cast<AnimationGraphEditor*>(
                m_Context.FindSubModule(AnimationGraphEditor::kModuleId)))
        {
            editor->NotifyGraphChanged();
        }
    }

    void AnimGraphWindow::PushStoredPositionsToEditor(AnimStateMachine& stateMachine)
    {
        LayoutNodesIfNeeded(stateMachine);

        for (size_t i = 0; i < stateMachine.States.size(); ++i)
        {
            const AnimState& state = stateMachine.States[i];
            Ed::SetNodePosition(
                AnimGraphIds::ToStateNodeId(i),
                ImVec2(state.EditorPosX, state.EditorPosY));
        }
    }

    void AnimGraphWindow::DrawNodes(AnimStateMachine& stateMachine)
    {
        constexpr float kNodeWidth = 180.0f;
        constexpr float kPinRadius = 4.0f;
        constexpr float kPinVisual = kPinRadius * 2.0f;

        auto drawEdgePinHotspot = [kPinRadius, kPinVisual](const ImColor& color)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            drawList->AddCircleFilled(
                ImVec2(cursor.x + kPinRadius, cursor.y + kPinRadius),
                kPinRadius,
                color);
            ImGui::Dummy(ImVec2(kPinVisual, kPinVisual));
        };

        for (size_t i = 0; i < stateMachine.States.size(); ++i)
        {
            const AnimState& state = stateMachine.States[i];
            const Ed::NodeId nodeId = AnimGraphIds::ToStateNodeId(i);

            ImGui::PushID(static_cast<int>(i));
            Ed::BeginNode(nodeId);
            ImGui::PushItemWidth(kNodeWidth);

            {
                EditorThemeScope nodeTitleTheme = EditorWindowTheme::PrimaryText(m_Context.GetEditorAppearance());
                ImGui::TextUnformatted(state.Name.empty() ? "(unnamed)" : state.Name.c_str());
            }

            const char* clipLabel = "(no clip)";
            if (state.Clip && state.Clip->GetMeta())
            {
                clipLabel = state.Clip->GetMeta()->AssetName.c_str();
            }
            ImGui::TextDisabled("%s", clipLabel);

            ImGui::Spacing();

            // L2: edge hotspots (no In/Out text labels).
            Ed::PushStyleColor(Ed::StyleColor_PinRect, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            Ed::PushStyleColor(Ed::StyleColor_PinRectBorder, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

            Ed::BeginPin(AnimGraphIds::ToStateInputPinId(i), Ed::PinKind::Input);
            Ed::PinPivotAlignment(ImVec2(0.0f, 0.5f));
            drawEdgePinHotspot(ImColor(120, 180, 220));
            Ed::EndPin();

            ImGui::SameLine(kNodeWidth - kPinVisual);

            Ed::BeginPin(AnimGraphIds::ToStateOutputPinId(i), Ed::PinKind::Output);
            Ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
            drawEdgePinHotspot(ImColor(220, 180, 100));
            Ed::EndPin();

            Ed::PopStyleColor(2);

            ImGui::PopItemWidth();
            Ed::EndNode();
            ImGui::PopID();
        }
    }

    void AnimGraphWindow::DrawLinks(AnimStateMachine& stateMachine)
    {
        // Draw only explicit From->To Transitions (skip AnyState edges on canvas).
        // Direction = Output pin → Input pin. Do NOT call Ed::Flow() every frame:
        // FlowAnimation keeps a Link* and UAF-crashes after AcceptDeletedItem
        // marks the link DeleteOnNewFrame (freed at next Editor Begin).
        Ed::PushStyleVar(Ed::StyleVar_LinkStrength, 80.0f);

        for (size_t i = 0; i < stateMachine.Transitions.size(); ++i)
        {
            const AnimTransition& transition = stateMachine.Transitions[i];

            size_t fromIndex = SIZE_MAX;
            size_t toIndex = SIZE_MAX;
            for (size_t stateIndex = 0; stateIndex < stateMachine.States.size(); ++stateIndex)
            {
                if (stateMachine.States[stateIndex].Name == transition.FromStateName)
                {
                    fromIndex = stateIndex;
                }
                if (stateMachine.States[stateIndex].Name == transition.ToStateName)
                {
                    toIndex = stateIndex;
                }
            }

            if (fromIndex == SIZE_MAX || toIndex == SIZE_MAX)
            {
                continue;
            }

            Ed::Link(
                AnimGraphIds::ToTransitionLinkId(i),
                AnimGraphIds::ToStateOutputPinId(fromIndex),
                AnimGraphIds::ToStateInputPinId(toIndex),
                ImVec4(0.85f, 0.85f, 0.90f, 1.0f),
                2.0f);
        }

        Ed::PopStyleVar();
    }

    void AnimGraphWindow::SyncSelectionFromEditor(AnimStateMachine& stateMachine)
    {
        AnimationGraphEditor* animGraphEditor = dynamic_cast<AnimationGraphEditor*>(
            m_Context.FindSubModule(AnimationGraphEditor::kModuleId));
        if (!animGraphEditor)
        {
            return;
        }

        const int selectedCount = Ed::GetSelectedObjectCount();
        if (selectedCount != 1)
        {
            animGraphEditor->ClearSelection();
            return;
        }

        Ed::LinkId selectedLinkId;
        if (Ed::GetSelectedLinks(&selectedLinkId, 1) == 1)
        {
            size_t transitionIndex = 0;
            if (AnimGraphIds::FromTransitionLinkId(selectedLinkId, transitionIndex)
                && transitionIndex < stateMachine.Transitions.size())
            {
                AnimGraphSelection selection;
                selection.Kind = AnimGraphSelectionKind::Transition;
                selection.TransitionIndex = static_cast<int>(transitionIndex);
                animGraphEditor->SetSelection(std::move(selection));
                return;
            }

            size_t anyIndex = 0;
            if (AnimGraphIds::FromAnyStateLinkId(selectedLinkId, anyIndex)
                && anyIndex < stateMachine.AnyStateTransitions.size())
            {
                AnimGraphSelection selection;
                selection.Kind = AnimGraphSelectionKind::AnyStateTransition;
                selection.TransitionIndex = static_cast<int>(anyIndex);
                animGraphEditor->SetSelection(std::move(selection));
                return;
            }

            animGraphEditor->ClearSelection();
            return;
        }

        Ed::NodeId selectedNodeId;
        if (Ed::GetSelectedNodes(&selectedNodeId, 1) == 1)
        {
            size_t stateIndex = 0;
            if (AnimGraphIds::FromStateNodeId(selectedNodeId, stateIndex)
                && stateIndex < stateMachine.States.size())
            {
                AnimGraphSelection selection;
                selection.Kind = AnimGraphSelectionKind::State;
                selection.StateName = stateMachine.States[stateIndex].Name;
                animGraphEditor->SetSelection(std::move(selection));
                return;
            }
        }

        animGraphEditor->ClearSelection();
    }

    void AnimGraphWindow::HandleCreateLink(AnimationGraph& graph)
    {
        // BeginCreate() may return false while still setting CreateItemAction::m_InActive;
        // EndCreate() must always run to pair with the internal Begin().
        if (!Ed::BeginCreate())
        {
            Ed::EndCreate();
            return;
        }

        AnimationGraphEditor* animGraphEditor = dynamic_cast<AnimationGraphEditor*>(
            m_Context.FindSubModule(AnimationGraphEditor::kModuleId));

        Ed::PinId startPinId;
        Ed::PinId endPinId;
        if (Ed::QueryNewLink(&startPinId, &endPinId) && animGraphEditor)
        {
            size_t startStateIndex = 0;
            size_t endStateIndex = 0;
            Ed::PinKind startKind = Ed::PinKind::Input;
            Ed::PinKind endKind = Ed::PinKind::Input;

            const bool startOk = AnimGraphIds::FromStatePinId(startPinId, startStateIndex, startKind);
            const bool endOk = AnimGraphIds::FromStatePinId(endPinId, endStateIndex, endKind);

            size_t fromIndex = SIZE_MAX;
            size_t toIndex = SIZE_MAX;
            if (startOk && endOk)
            {
                if (startKind == Ed::PinKind::Output && endKind == Ed::PinKind::Input)
                {
                    fromIndex = startStateIndex;
                    toIndex = endStateIndex;
                }
                else if (startKind == Ed::PinKind::Input && endKind == Ed::PinKind::Output)
                {
                    fromIndex = endStateIndex;
                    toIndex = startStateIndex;
                }
            }

            AnimStateMachine& stateMachine = graph.GetStateMachine();
            const bool indicesValid =
                fromIndex != SIZE_MAX
                && toIndex != SIZE_MAX
                && fromIndex < stateMachine.States.size()
                && toIndex < stateMachine.States.size()
                && fromIndex != toIndex;

            if (indicesValid)
            {
                if (Ed::AcceptNewItem())
                {
                    animGraphEditor->AddTransition(
                        stateMachine.States[fromIndex].Name,
                        stateMachine.States[toIndex].Name);
                }
            }
            else
            {
                Ed::RejectNewItem();
            }
        }

        Ed::EndCreate();
    }

    void AnimGraphWindow::HandleDelete(AnimationGraph& graph)
    {
        if (!Ed::BeginDelete())
        {
            return;
        }

        AnimationGraphEditor* animGraphEditor = dynamic_cast<AnimationGraphEditor*>(
            m_Context.FindSubModule(AnimationGraphEditor::kModuleId));
        if (!animGraphEditor)
        {
            Ed::EndDelete();
            return;
        }

        AnimStateMachine& stateMachine = graph.GetStateMachine();
        std::vector<size_t> transitionIndices;
        std::vector<size_t> anyTransitionIndices;

        // Do not request start/end pin ids: QueryDeletedLink's FindLink path can
        // null-deref if the candidate is already marked deleted.
        Ed::LinkId linkId;
        while (Ed::QueryDeletedLink(&linkId))
        {
            size_t transitionIndex = 0;
            if (AnimGraphIds::FromTransitionLinkId(linkId, transitionIndex)
                && transitionIndex < stateMachine.Transitions.size())
            {
                transitionIndices.push_back(transitionIndex);
                Ed::AcceptDeletedItem(false);
                continue;
            }

            size_t anyIndex = 0;
            if (AnimGraphIds::FromAnyStateLinkId(linkId, anyIndex)
                && anyIndex < stateMachine.AnyStateTransitions.size())
            {
                anyTransitionIndices.push_back(anyIndex);
                Ed::AcceptDeletedItem(false);
                continue;
            }

            Ed::RejectDeletedItem();
        }

        std::sort(transitionIndices.begin(), transitionIndices.end(), std::greater<size_t>());
        transitionIndices.erase(
            std::unique(transitionIndices.begin(), transitionIndices.end()),
            transitionIndices.end());
        for (size_t index : transitionIndices)
        {
            animGraphEditor->RemoveTransitionAt(index);
        }

        std::sort(anyTransitionIndices.begin(), anyTransitionIndices.end(), std::greater<size_t>());
        anyTransitionIndices.erase(
            std::unique(anyTransitionIndices.begin(), anyTransitionIndices.end()),
            anyTransitionIndices.end());
        for (size_t index : anyTransitionIndices)
        {
            animGraphEditor->RemoveAnyStateTransitionAt(index);
        }

        std::vector<std::string> stateNamesToRemove;
        Ed::NodeId deletedNodeId;
        while (Ed::QueryDeletedNode(&deletedNodeId))
        {
            size_t stateIndex = 0;
            if (!AnimGraphIds::FromStateNodeId(deletedNodeId, stateIndex)
                || stateIndex >= stateMachine.States.size())
            {
                Ed::RejectDeletedItem();
                continue;
            }

            stateNamesToRemove.push_back(stateMachine.States[stateIndex].Name);
            Ed::AcceptDeletedItem(false);
        }

        for (const std::string& stateName : stateNamesToRemove)
        {
            animGraphEditor->RemoveStateByName(stateName);
        }

        // Any structural delete invalidates selection / link-id mapping.
        if (!transitionIndices.empty() || !anyTransitionIndices.empty() || !stateNamesToRemove.empty())
        {
            animGraphEditor->ClearSelection();
        }

        Ed::EndDelete();
    }

    void AnimGraphWindow::SyncNodePositions(AnimStateMachine& stateMachine)
    {
        AnimationGraphEditor* animGraphEditor = dynamic_cast<AnimationGraphEditor*>(
            m_Context.FindSubModule(AnimationGraphEditor::kModuleId));

        bool changed = false;
        for (size_t i = 0; i < stateMachine.States.size(); ++i)
        {
            AnimState& state = stateMachine.States[i];
            const ImVec2 position = Ed::GetNodePosition(AnimGraphIds::ToStateNodeId(i));
            if (state.EditorPosX != position.x || state.EditorPosY != position.y)
            {
                state.EditorPosX = position.x;
                state.EditorPosY = position.y;
                changed = true;
            }
        }

        if (changed && animGraphEditor)
        {
            animGraphEditor->NotifyGraphChanged();
        }
    }

    void AnimGraphWindow::DrawBackgroundContextMenu(AnimationGraph& graph)
    {
        (void)graph;

        AnimationGraphEditor* animGraphEditor = dynamic_cast<AnimationGraphEditor*>(
            m_Context.FindSubModule(AnimationGraphEditor::kModuleId));
        if (!animGraphEditor)
        {
            return;
        }

        // Persist click canvas position across the popup open frame.
        static ImVec2 s_PendingAddCanvasPos(0.0f, 0.0f);

        Ed::Suspend();
        if (Ed::ShowBackgroundContextMenu())
        {
            s_PendingAddCanvasPos = Ed::ScreenToCanvas(ImGui::GetMousePos());
            ImGui::OpenPopup("AnimGraphBackgroundContext");
        }

        if (ImGui::BeginPopup("AnimGraphBackgroundContext"))
        {
            if (ImGui::MenuItem("Add State"))
            {
                animGraphEditor->AddStateAt(s_PendingAddCanvasPos.x, s_PendingAddCanvasPos.y);
            }
            ImGui::EndPopup();
        }
        Ed::Resume();
    }
}
