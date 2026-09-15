#include "AnimGraphSmBridge.h"

#include "AnimationGraphEditor.h"

#include "UI/Appearance/EditorAppearance.h"

#include "imgui.h"

#include "Runtime/Resource/AssetMeta.h"

#include <cmath>

namespace minEngine
{
    SmGraph::NodeId AnimGraphSmBridge::ToNodeId(size_t stateIndex)
    {
        return static_cast<SmGraph::NodeId>(stateIndex + 1);
    }

    SmGraph::EdgeId AnimGraphSmBridge::ToEdgeId(size_t transitionIndex)
    {
        return static_cast<SmGraph::EdgeId>(transitionIndex + 1);
    }

    SmGraph::EdgeId AnimGraphSmBridge::ToAnyStateEdgeId(size_t anyTransitionIndex)
    {
        return SmGraph::kAnyStateEdgeBase + static_cast<SmGraph::EdgeId>(anyTransitionIndex);
    }

    bool AnimGraphSmBridge::FromNodeId(SmGraph::NodeId id, size_t& outStateIndex)
    {
        if (id == SmGraph::kInvalidNodeId
            || id == SmGraph::kEntryNodeId
            || id == SmGraph::kAnyStateNodeId)
        {
            return false;
        }
        outStateIndex = static_cast<size_t>(id - 1);
        return true;
    }

    bool AnimGraphSmBridge::FromEdgeId(SmGraph::EdgeId id, size_t& outTransitionIndex)
    {
        if (id == SmGraph::kInvalidEdgeId
            || id == SmGraph::kEntryEdgeId
            || id >= SmGraph::kAnyStateEdgeBase)
        {
            return false;
        }
        outTransitionIndex = static_cast<size_t>(id - 1);
        return true;
    }

    bool AnimGraphSmBridge::FromAnyStateEdgeId(SmGraph::EdgeId id, size_t& outAnyTransitionIndex)
    {
        if (id < SmGraph::kAnyStateEdgeBase || id >= SmGraph::kAnyStateEdgeEnd)
        {
            return false;
        }
        outAnyTransitionIndex = static_cast<size_t>(id - SmGraph::kAnyStateEdgeBase);
        return true;
    }

    void AnimGraphSmBridge::PullDocument(
        const AnimationGraph& graph,
        SmGraph::Document& outDocument,
        AnimGraphSpecialNodeLayout& specialLayout)
    {
        const SmGraph::Selection previousSelection = outDocument.GetSelection();
        outDocument.Clear();

        if (!specialLayout.Initialized)
        {
            specialLayout.EntryPos = ImVec2(-200.0f, 40.0f);
            specialLayout.AnyStatePos = ImVec2(-200.0f, 200.0f);
            specialLayout.Initialized = true;
        }

        const AnimStateMachine& stateMachine = graph.GetStateMachine();
        outDocument.GetNodes().reserve(stateMachine.States.size() + 2);

        {
            SmGraph::Node entry;
            entry.Id = SmGraph::kEntryNodeId;
            entry.Kind = SmGraph::NodeKind::Entry;
            entry.Pos = specialLayout.EntryPos;
            entry.Title = "Entry";
            entry.Subtitle.clear();
            entry.Deletable = false;
            outDocument.GetNodes().push_back(std::move(entry));
        }

        {
            SmGraph::Node anyState;
            anyState.Id = SmGraph::kAnyStateNodeId;
            anyState.Kind = SmGraph::NodeKind::AnyState;
            anyState.Pos = specialLayout.AnyStatePos;
            anyState.Title = "Any State";
            anyState.Subtitle.clear();
            anyState.Deletable = false;
            outDocument.GetNodes().push_back(std::move(anyState));
        }

        for (size_t i = 0; i < stateMachine.States.size(); ++i)
        {
            const AnimState& state = stateMachine.States[i];
            SmGraph::Node node;
            node.Id = ToNodeId(i);
            node.Kind = SmGraph::NodeKind::State;
            node.Pos = ImVec2(state.EditorPosX, state.EditorPosY);
            node.Size = ImVec2(160.0f, 56.0f);
            node.Title = state.Name.empty() ? "(unnamed)" : state.Name;
            if (state.Clip && state.Clip->GetMeta())
            {
                node.Subtitle = state.Clip->GetMeta()->AssetName;
            }
            else
            {
                node.Subtitle = "(no clip)";
            }
            node.Deletable = true;
            outDocument.GetNodes().push_back(std::move(node));
        }

        outDocument.GetEdges().reserve(
            stateMachine.Transitions.size() + stateMachine.AnyStateTransitions.size() + 1);

        if (!stateMachine.DefaultStateName.empty())
        {
            for (size_t stateIndex = 0; stateIndex < stateMachine.States.size(); ++stateIndex)
            {
                if (stateMachine.States[stateIndex].Name == stateMachine.DefaultStateName)
                {
                    SmGraph::Edge entryEdge;
                    entryEdge.Id = SmGraph::kEntryEdgeId;
                    entryEdge.Kind = SmGraph::EdgeKind::EntryDefault;
                    entryEdge.From = SmGraph::kEntryNodeId;
                    entryEdge.To = ToNodeId(stateIndex);
                    entryEdge.CanReverse = false;
                    outDocument.GetEdges().push_back(entryEdge);
                    break;
                }
            }
        }

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

            SmGraph::Edge edge;
            edge.Id = ToEdgeId(i);
            edge.Kind = SmGraph::EdgeKind::Transition;
            edge.From = ToNodeId(fromIndex);
            edge.To = ToNodeId(toIndex);
            edge.CanReverse = true;
            outDocument.GetEdges().push_back(edge);
        }

        for (size_t i = 0; i < stateMachine.AnyStateTransitions.size(); ++i)
        {
            const AnimTransition& transition = stateMachine.AnyStateTransitions[i];

            size_t toIndex = SIZE_MAX;
            for (size_t stateIndex = 0; stateIndex < stateMachine.States.size(); ++stateIndex)
            {
                if (stateMachine.States[stateIndex].Name == transition.ToStateName)
                {
                    toIndex = stateIndex;
                    break;
                }
            }
            if (toIndex == SIZE_MAX)
            {
                continue;
            }

            SmGraph::Edge edge;
            edge.Id = ToAnyStateEdgeId(i);
            edge.Kind = SmGraph::EdgeKind::AnyState;
            edge.From = SmGraph::kAnyStateNodeId;
            edge.To = ToNodeId(toIndex);
            edge.CanReverse = false;
            outDocument.GetEdges().push_back(edge);
        }

        switch (previousSelection.Kind)
        {
        case SmGraph::SelectionKind::Node:
            if (outDocument.FindNode(previousSelection.Node))
            {
                outDocument.GetSelection() = previousSelection;
            }
            break;
        case SmGraph::SelectionKind::Edge:
            if (outDocument.FindEdge(previousSelection.Edge))
            {
                outDocument.GetSelection() = previousSelection;
            }
            break;
        default:
            break;
        }
    }

    bool AnimGraphSmBridge::PushPositions(
        const SmGraph::Document& document,
        AnimationGraph& graph,
        AnimGraphSpecialNodeLayout& specialLayout)
    {
        AnimStateMachine& stateMachine = graph.GetStateMachine();
        bool changed = false;

        for (const SmGraph::Node& node : document.GetNodes())
        {
            if (node.Id == SmGraph::kEntryNodeId)
            {
                if (specialLayout.EntryPos.x != node.Pos.x || specialLayout.EntryPos.y != node.Pos.y)
                {
                    specialLayout.EntryPos = node.Pos;
                    // Session-only; do not dirty the asset for decorative moves.
                }
                continue;
            }
            if (node.Id == SmGraph::kAnyStateNodeId)
            {
                if (specialLayout.AnyStatePos.x != node.Pos.x
                    || specialLayout.AnyStatePos.y != node.Pos.y)
                {
                    specialLayout.AnyStatePos = node.Pos;
                }
                continue;
            }

            size_t stateIndex = 0;
            if (!FromNodeId(node.Id, stateIndex) || stateIndex >= stateMachine.States.size())
            {
                continue;
            }

            AnimState& state = stateMachine.States[stateIndex];
            if (state.EditorPosX != node.Pos.x || state.EditorPosY != node.Pos.y)
            {
                state.EditorPosX = node.Pos.x;
                state.EditorPosY = node.Pos.y;
                changed = true;
            }
        }

        return changed;
    }

    bool AnimGraphSmBridge::LayoutIfNeeded(AnimStateMachine& stateMachine)
    {
        if (stateMachine.States.empty())
        {
            return false;
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
            return false;
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

        return true;
    }

    void AnimGraphSmBridge::ApplyEditEvents(
        AnimationGraphEditor& editor,
        AnimationGraph& graph,
        SmGraph::Document& document,
        AnimGraphSpecialNodeLayout& specialLayout,
        const std::vector<SmGraph::EditEvent>& events)
    {
        AnimStateMachine& stateMachine = graph.GetStateMachine();
        bool structureChanged = false;

        for (const SmGraph::EditEvent& event : events)
        {
            switch (event.Kind)
            {
            case SmGraph::EditKind::SelectionChanged:
            {
                AnimGraphSelection selection;
                if (document.GetSelection().Kind == SmGraph::SelectionKind::Node)
                {
                    const SmGraph::NodeId nodeId = document.GetSelection().Node;
                    if (nodeId == SmGraph::kEntryNodeId || nodeId == SmGraph::kAnyStateNodeId)
                    {
                        editor.ClearSelection();
                        break;
                    }

                    size_t stateIndex = 0;
                    if (FromNodeId(nodeId, stateIndex)
                        && stateIndex < stateMachine.States.size())
                    {
                        selection.Kind = AnimGraphSelectionKind::State;
                        selection.StateName = stateMachine.States[stateIndex].Name;
                    }
                }
                else if (document.GetSelection().Kind == SmGraph::SelectionKind::Edge)
                {
                    const SmGraph::EdgeId edgeId = document.GetSelection().Edge;
                    if (edgeId == SmGraph::kEntryEdgeId)
                    {
                        editor.ClearSelection();
                        break;
                    }

                    size_t transitionIndex = 0;
                    if (FromEdgeId(edgeId, transitionIndex)
                        && transitionIndex < stateMachine.Transitions.size())
                    {
                        selection.Kind = AnimGraphSelectionKind::Transition;
                        selection.TransitionIndex = static_cast<int>(transitionIndex);
                    }
                    else if (FromAnyStateEdgeId(edgeId, transitionIndex)
                        && transitionIndex < stateMachine.AnyStateTransitions.size())
                    {
                        selection.Kind = AnimGraphSelectionKind::AnyStateTransition;
                        selection.TransitionIndex = static_cast<int>(transitionIndex);
                    }
                }
                editor.SetSelection(std::move(selection));
                break;
            }
            case SmGraph::EditKind::NodeMoved:
                break;
            case SmGraph::EditKind::CreateEdgeRequested:
            {
                if (event.From == SmGraph::kEntryNodeId)
                {
                    size_t toIndex = 0;
                    if (FromNodeId(event.To, toIndex) && toIndex < stateMachine.States.size())
                    {
                        const std::string toName = stateMachine.States[toIndex].Name;
                        if (editor.SubmitOwnedPropertyMutation(
                                "m_StateMachine",
                                [&]() { return editor.SetDefaultStateName(toName); }))
                        {
                            structureChanged = true;
                        }
                    }
                    break;
                }

                if (event.From == SmGraph::kAnyStateNodeId)
                {
                    size_t toIndex = 0;
                    if (FromNodeId(event.To, toIndex) && toIndex < stateMachine.States.size())
                    {
                        const std::string toName = stateMachine.States[toIndex].Name;
                        if (editor.SubmitOwnedPropertyMutation(
                                "m_StateMachine",
                                [&]() { return editor.AddAnyStateTransition(toName); }))
                        {
                            structureChanged = true;
                        }
                    }
                    break;
                }

                size_t fromIndex = 0;
                size_t toIndex = 0;
                if (!FromNodeId(event.From, fromIndex) || !FromNodeId(event.To, toIndex)
                    || fromIndex >= stateMachine.States.size()
                    || toIndex >= stateMachine.States.size()
                    || fromIndex == toIndex)
                {
                    break;
                }

                const std::string fromName = stateMachine.States[fromIndex].Name;
                const std::string toName = stateMachine.States[toIndex].Name;
                if (editor.SubmitOwnedPropertyMutation(
                        "m_StateMachine",
                        [&]() { return editor.AddTransition(fromName, toName); }))
                {
                    structureChanged = true;
                }
                break;
            }
            case SmGraph::EditKind::DeleteEdgeRequested:
            {
                size_t transitionIndex = 0;
                if (FromEdgeId(event.Edge, transitionIndex)
                    && transitionIndex < stateMachine.Transitions.size())
                {
                    if (editor.SubmitOwnedPropertyMutation(
                            "m_StateMachine",
                            [&]() { return editor.RemoveTransitionAt(transitionIndex); }))
                    {
                        structureChanged = true;
                    }
                }
                else if (FromAnyStateEdgeId(event.Edge, transitionIndex)
                    && transitionIndex < stateMachine.AnyStateTransitions.size())
                {
                    if (editor.SubmitOwnedPropertyMutation(
                            "m_StateMachine",
                            [&]() { return editor.RemoveAnyStateTransitionAt(transitionIndex); }))
                    {
                        structureChanged = true;
                    }
                }
                break;
            }
            case SmGraph::EditKind::ReverseEdgeRequested:
            {
                size_t transitionIndex = 0;
                if (!FromEdgeId(event.Edge, transitionIndex)
                    || transitionIndex >= stateMachine.Transitions.size())
                {
                    break;
                }

                AnimGraphSelection selection;
                selection.Kind = AnimGraphSelectionKind::Transition;
                selection.TransitionIndex = static_cast<int>(transitionIndex);
                editor.SetSelection(std::move(selection));
                if (editor.SubmitOwnedPropertyMutation(
                        "m_StateMachine",
                        [&]() { return editor.ReverseTransition(); }))
                {
                    structureChanged = true;
                }
                break;
            }
            case SmGraph::EditKind::DeleteSelectionRequested:
            {
                if (document.GetSelection().Kind == SmGraph::SelectionKind::Edge)
                {
                    const SmGraph::EdgeId edgeId = document.GetSelection().Edge;
                    if (edgeId == SmGraph::kEntryEdgeId)
                    {
                        break;
                    }

                    size_t transitionIndex = 0;
                    if (FromEdgeId(edgeId, transitionIndex)
                        && transitionIndex < stateMachine.Transitions.size())
                    {
                        if (editor.SubmitOwnedPropertyMutation(
                                "m_StateMachine",
                                [&]() { return editor.RemoveTransitionAt(transitionIndex); }))
                        {
                            structureChanged = true;
                        }
                    }
                    else if (FromAnyStateEdgeId(edgeId, transitionIndex)
                        && transitionIndex < stateMachine.AnyStateTransitions.size())
                    {
                        if (editor.SubmitOwnedPropertyMutation(
                                "m_StateMachine",
                                [&]() { return editor.RemoveAnyStateTransitionAt(transitionIndex); }))
                        {
                            structureChanged = true;
                        }
                    }
                }
                else if (document.GetSelection().Kind == SmGraph::SelectionKind::Node)
                {
                    const SmGraph::Node* node = document.FindNode(document.GetSelection().Node);
                    if (!node || !node->Deletable)
                    {
                        break;
                    }

                    size_t stateIndex = 0;
                    if (FromNodeId(node->Id, stateIndex)
                        && stateIndex < stateMachine.States.size())
                    {
                        const std::string name = stateMachine.States[stateIndex].Name;
                        if (editor.SubmitOwnedPropertyMutation(
                                "m_StateMachine",
                                [&]() { return editor.RemoveStateByName(name); }))
                        {
                            structureChanged = true;
                        }
                    }
                }
                break;
            }
            case SmGraph::EditKind::AddNodeRequested:
                if (editor.SubmitOwnedPropertyMutation(
                        "m_StateMachine",
                        [&]() { return editor.AddStateAt(event.Pos.x, event.Pos.y); }))
                {
                    structureChanged = true;
                }
                break;
            case SmGraph::EditKind::RenameNodeRequested:
            {
                size_t stateIndex = 0;
                if (!FromNodeId(event.Node, stateIndex)
                    || stateIndex >= stateMachine.States.size())
                {
                    break;
                }
                const std::string oldName = stateMachine.States[stateIndex].Name;
                if (editor.SubmitOwnedPropertyMutation(
                        "m_StateMachine",
                        [&]() { return editor.RenameState(oldName, event.Text); }))
                {
                    structureChanged = true;
                }
                break;
            }
            }
        }

        if (!structureChanged)
        {
            if (!editor.HasPositionDragBefore() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                editor.CapturePositionDragBefore();
            }

            if (PushPositions(document, graph, specialLayout))
            {
                editor.NotifyGraphChanged();
            }

            editor.CommitPositionDragIfNeeded();
        }
        else
        {
            // Keep session positions even when only special nodes moved.
            PushPositions(document, graph, specialLayout);
            editor.ClearSelection();
            document.GetSelection().Clear();
            PullDocument(graph, document, specialLayout);
        }
    }

    void AnimGraphSmBridge::ApplyEditorTheme(const EditorAppearance& appearance, SmGraph::Style& outStyle)
    {
        const EditorThemePalette& palette = appearance.GetActivePalette();

        outStyle.GridColor = appearance.GetDisplayColorU32(palette.Separator, 0.40f);

        const ImVec4 field = appearance.GetDisplayColor(palette.FieldBackground);
        const float fieldLuma = 0.2126f * field.x + 0.7152f * field.y + 0.0722f * field.z;
        const bool lightChrome = fieldLuma > 0.55f;

        const ImVec4 accentBlue = lightChrome
            ? ImVec4(0.26f, 0.45f, 0.85f, 1.0f)
            : ImVec4(0.35f, 0.62f, 0.98f, 1.0f);
        const ImVec4 accentWarm = lightChrome
            ? ImVec4(0.85f, 0.55f, 0.12f, 1.0f)
            : ImVec4(0.95f, 0.72f, 0.28f, 1.0f);
        const ImVec4 edgeBright = lightChrome
            ? ImVec4(0.35f, 0.45f, 0.62f, 0.95f)
            : ImVec4(0.72f, 0.78f, 0.90f, 0.95f);
        const ImVec4 ringSelected = lightChrome
            ? ImVec4(0.78f, 0.86f, 0.98f, 1.0f)
            : ImVec4(0.22f, 0.32f, 0.48f, 1.0f);
        const ImVec4 entryRing = lightChrome
            ? ImVec4(0.72f, 0.88f, 0.78f, 1.0f)
            : ImVec4(0.20f, 0.34f, 0.26f, 1.0f);
        const ImVec4 anyRing = lightChrome
            ? ImVec4(0.92f, 0.82f, 0.72f, 1.0f)
            : ImVec4(0.34f, 0.26f, 0.20f, 1.0f);
        const ImVec4 entryEdge = lightChrome
            ? ImVec4(0.25f, 0.55f, 0.35f, 0.95f)
            : ImVec4(0.45f, 0.80f, 0.55f, 0.90f);
        const ImVec4 anyEdge = lightChrome
            ? ImVec4(0.70f, 0.45f, 0.20f, 0.95f)
            : ImVec4(0.90f, 0.65f, 0.40f, 0.90f);

        if (lightChrome)
        {
            outStyle.NodeRingFill = appearance.GetDisplayColorU32(palette.Accent);
            outStyle.NodeBodyFill = appearance.GetDisplayColorU32(palette.FieldBackground);
        }
        else
        {
            outStyle.NodeRingFill = appearance.GetDisplayColorU32(palette.FieldBackground);
            outStyle.NodeBodyFill = appearance.GetDisplayColorU32(palette.PopupBackground);
        }

        outStyle.NodeRingFillSelected = ImGui::ColorConvertFloat4ToU32(ringSelected);
        outStyle.NodeBodyBorder = appearance.GetDisplayColorU32(palette.Separator);
        outStyle.NodeBorder = appearance.GetDisplayColorU32(palette.Border);
        outStyle.NodeBorderSelected = ImGui::ColorConvertFloat4ToU32(accentBlue);
        outStyle.NodeTitle = appearance.GetDisplayColorU32(palette.TextPrimary);
        outStyle.NodeSubtitle = appearance.GetDisplayColorU32(palette.TextMuted);
        outStyle.EntryRingFill = ImGui::ColorConvertFloat4ToU32(entryRing);
        outStyle.AnyStateRingFill = ImGui::ColorConvertFloat4ToU32(anyRing);
        outStyle.EdgeColor = ImGui::ColorConvertFloat4ToU32(edgeBright);
        outStyle.EdgeSelected = ImGui::ColorConvertFloat4ToU32(accentBlue);
        outStyle.EntryEdgeColor = ImGui::ColorConvertFloat4ToU32(entryEdge);
        outStyle.AnyStateEdgeColor = ImGui::ColorConvertFloat4ToU32(anyEdge);
        outStyle.LinkPreview = ImGui::ColorConvertFloat4ToU32(accentWarm);
        outStyle.HoverTarget = ImGui::ColorConvertFloat4ToU32(accentWarm);
    }
}
