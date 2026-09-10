#pragma once

#include "UI/SmGraph/SmGraphTypes.h"
#include "UI/SmGraph/SmGraphWidget.h"

#include "Runtime/Function/Animation/AnimationGraph.h"

namespace minEngine
{
    class AnimationGraphEditor;

    /** Layer 2: maps AnimationGraph truth <-> SmGraph::Document / EditEvent. */
    class AnimGraphSmBridge
    {
    public:
        static SmGraph::NodeId ToNodeId(size_t stateIndex);
        static SmGraph::EdgeId ToEdgeId(size_t transitionIndex);
        static bool FromNodeId(SmGraph::NodeId id, size_t& outStateIndex);
        static bool FromEdgeId(SmGraph::EdgeId id, size_t& outTransitionIndex);

        /** Rebuild document from graph (skips AnyState edges). */
        static void PullDocument(const AnimationGraph& graph, SmGraph::Document& outDocument);

        /** Write node positions back to EditorPos*. Returns true if any value changed. */
        static bool PushPositions(const SmGraph::Document& document, AnimationGraph& graph);

        /** Auto-layout when all EditorPos are zero. */
        static bool LayoutIfNeeded(AnimStateMachine& stateMachine);

        /** Map active EditorAppearance palette into SmGraph style (dark/light). */
        static void ApplyEditorTheme(const class EditorAppearance& appearance, SmGraph::Style& outStyle);

        static void ApplyEditEvents(
            AnimationGraphEditor& editor,
            AnimationGraph& graph,
            SmGraph::Document& document,
            const std::vector<SmGraph::EditEvent>& events);
    };
}