#pragma once

#include "UI/SmGraph/SmGraphTypes.h"
#include "UI/SmGraph/SmGraphWidget.h"

#include "Runtime/Function/Animation/AnimationGraph.h"

namespace minEngine
{
    class AnimationGraphEditor;

    /** Session-only positions for decorative nodes (not persisted in asset MVP). */
    struct AnimGraphSpecialNodeLayout
    {
        ImVec2 EntryPos{-200.0f, 40.0f};
        ImVec2 AnyStatePos{-200.0f, 200.0f};
        bool Initialized = false;
    };

    /** Layer 2: maps AnimationGraph truth <-> SmGraph::Document / EditEvent. */
    class AnimGraphSmBridge
    {
    public:
        static SmGraph::NodeId ToNodeId(size_t stateIndex);
        static SmGraph::EdgeId ToEdgeId(size_t transitionIndex);
        static SmGraph::EdgeId ToAnyStateEdgeId(size_t anyTransitionIndex);

        static bool FromNodeId(SmGraph::NodeId id, size_t& outStateIndex);
        static bool FromEdgeId(SmGraph::EdgeId id, size_t& outTransitionIndex);
        static bool FromAnyStateEdgeId(SmGraph::EdgeId id, size_t& outAnyTransitionIndex);

        /** Rebuild document from graph, including Entry / AnyState view nodes. */
        static void PullDocument(
            const AnimationGraph& graph,
            SmGraph::Document& outDocument,
            AnimGraphSpecialNodeLayout& specialLayout);

        /** Write state EditorPos* and special-node session positions. */
        static bool PushPositions(
            const SmGraph::Document& document,
            AnimationGraph& graph,
            AnimGraphSpecialNodeLayout& specialLayout);

        /** Auto-layout when all EditorPos are zero. */
        static bool LayoutIfNeeded(AnimStateMachine& stateMachine);

        /** Map active EditorAppearance palette into SmGraph style (dark/light). */
        static void ApplyEditorTheme(const class EditorAppearance& appearance, SmGraph::Style& outStyle);

        static void ApplyEditEvents(
            AnimationGraphEditor& editor,
            AnimationGraph& graph,
            SmGraph::Document& document,
            AnimGraphSpecialNodeLayout& specialLayout,
            const std::vector<SmGraph::EditEvent>& events);
    };
}
