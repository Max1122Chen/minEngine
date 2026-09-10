#include "AnimGraphSmBridge.h"

#include "AnimationGraphEditor.h"

#include "UI/Appearance/EditorAppearance.h"

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

    bool AnimGraphSmBridge::FromNodeId(SmGraph::NodeId id, size_t& outStateIndex)
    {
        if (id == SmGraph::kInvalidNodeId)
        {
            return false;
        }
        outStateIndex = static_cast<size_t>(id - 1);
        return true;
    }

    bool AnimGraphSmBridge::FromEdgeId(SmGraph::EdgeId id, size_t& outTransitionIndex)
    {
        if (id == SmGraph::kInvalidEdgeId)
        {
            return false;
        }
        outTransitionIndex = static_cast<size_t>(id - 1);
        return true;
    }

    void AnimGraphSmBridge::PullDocument(const AnimationGraph& graph, SmGraph::Document& outDocument)
    {
        const SmGraph::Selection previousSelection = outDocument.GetSelection();
        outDocument.Clear();

        const AnimStateMachine& stateMachine = graph.GetStateMachine();
        outDocument.GetNodes().reserve(stateMachine.States.size());
        for (size_t i = 0; i < stateMachine.States.size(); ++i)
        {
            const AnimState& state = stateMachine.States[i];
            SmGraph::Node node;
            node.Id = ToNodeId(i);
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
            outDocument.GetNodes().push_back(std::move(node));
        }

        outDocument.GetEdges().reserve(stateMachine.Transitions.size());
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
            edge.From = ToNodeId(fromIndex);
            edge.To = ToNodeId(toIndex);
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

    bool AnimGraphSmBridge::PushPositions(const SmGraph::Document& document, AnimationGraph& graph)
    {
        AnimStateMachine& stateMachine = graph.GetStateMachine();
        bool changed = false;

        for (const SmGraph::Node& node : document.GetNodes())
        {
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
                    size_t stateIndex = 0;
                    if (FromNodeId(document.GetSelection().Node, stateIndex)
                        && stateIndex < stateMachine.States.size())
                    {
                        selection.Kind = AnimGraphSelectionKind::State;
                        selection.StateName = stateMachine.States[stateIndex].Name;
                    }
                }
                else if (document.GetSelection().Kind == SmGraph::SelectionKind::Edge)
                {
                    size_t transitionIndex = 0;
                    if (FromEdgeId(document.GetSelection().Edge, transitionIndex)
                        && transitionIndex < stateMachine.Transitions.size())
                    {
                        selection.Kind = AnimGraphSelectionKind::Transition;
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
                size_t fromIndex = 0;
                size_t toIndex = 0;
                if (!FromNodeId(event.From, fromIndex) || !FromNodeId(event.To, toIndex)
                    || fromIndex >= stateMachine.States.size()
                    || toIndex >= stateMachine.States.size()
                    || fromIndex == toIndex)
                {
                    break;
                }

                if (editor.AddTransition(
                        stateMachine.States[fromIndex].Name,
                        stateMachine.States[toIndex].Name))
                {
                    structureChanged = true;
                }
                break;
            }
            case SmGraph::EditKind::DeleteSelectionRequested:
            {
                if (document.GetSelection().Kind == SmGraph::SelectionKind::Edge)
                {
                    size_t transitionIndex = 0;
                    if (FromEdgeId(document.GetSelection().Edge, transitionIndex)
                        && transitionIndex < stateMachine.Transitions.size())
                    {
                        editor.RemoveTransitionAt(transitionIndex);
                        structureChanged = true;
                    }
                }
                else if (document.GetSelection().Kind == SmGraph::SelectionKind::Node)
                {
                    size_t stateIndex = 0;
                    if (FromNodeId(document.GetSelection().Node, stateIndex)
                        && stateIndex < stateMachine.States.size())
                    {
                        const std::string name = stateMachine.States[stateIndex].Name;
                        editor.RemoveStateByName(name);
                        structureChanged = true;
                    }
                }
                break;
            }
            case SmGraph::EditKind::AddNodeRequested:
                if (editor.AddStateAt(event.Pos.x, event.Pos.y))
                {
                    structureChanged = true;
                }
                break;
            }
        }

        if (!structureChanged && PushPositions(document, graph))
        {
            editor.NotifyGraphChanged();
        }

        if (structureChanged)
        {
            editor.ClearSelection();
            document.GetSelection().Clear();
            PullDocument(graph, document);
        }
    }


    void AnimGraphSmBridge::ApplyEditorTheme(const EditorAppearance& appearance, SmGraph::Style& outStyle)
    {
        const EditorThemePalette& palette = appearance.GetActivePalette();

        outStyle.GridColor = appearance.GetDisplayColorU32(palette.Separator, 0.40f);

        const ImVec4 field = appearance.GetDisplayColor(palette.FieldBackground);
        const float fieldLuma = 0.2126f * field.x + 0.7152f * field.y + 0.0722f * field.z;
        const bool lightChrome = fieldLuma > 0.55f;

        // Editor-style accents (not tied to the achromatic dark chrome palette).
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
        outStyle.EdgeColor = ImGui::ColorConvertFloat4ToU32(edgeBright);
        outStyle.EdgeSelected = ImGui::ColorConvertFloat4ToU32(accentBlue);
        outStyle.LinkPreview = ImGui::ColorConvertFloat4ToU32(accentWarm);
        outStyle.HoverTarget = ImGui::ColorConvertFloat4ToU32(accentWarm);
    }

}
