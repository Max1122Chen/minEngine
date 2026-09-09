#pragma once

#include "imgui_node_editor.h"

#include <cstddef>
#include <cstdint>

namespace minEngine
{
    /** Deterministic ax::NodeEditor IDs for AnimationGraph state/transition projection. */
    class AnimGraphIds
    {
    public:
        static constexpr int64_t kTransitionLinkBase = 100000;
        static constexpr int64_t kAnyStateLinkBase = 200000;

        static ax::NodeEditor::NodeId ToStateNodeId(size_t stateIndex);
        static bool FromStateNodeId(ax::NodeEditor::NodeId id, size_t& outStateIndex);

        static ax::NodeEditor::PinId ToStateInputPinId(size_t stateIndex);
        static ax::NodeEditor::PinId ToStateOutputPinId(size_t stateIndex);
        static bool FromStatePinId(
            ax::NodeEditor::PinId id,
            size_t& outStateIndex,
            ax::NodeEditor::PinKind& outKind);

        static ax::NodeEditor::LinkId ToTransitionLinkId(size_t transitionIndex);
        static bool FromTransitionLinkId(ax::NodeEditor::LinkId id, size_t& outTransitionIndex);

        static ax::NodeEditor::LinkId ToAnyStateLinkId(size_t anyTransitionIndex);
        static bool FromAnyStateLinkId(ax::NodeEditor::LinkId id, size_t& outAnyTransitionIndex);
    };
}
