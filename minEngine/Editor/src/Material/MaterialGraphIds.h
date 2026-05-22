#pragma once

#include <cstdint>

namespace ax::NodeEditor
{
    struct LinkId;
    struct NodeId;
    struct PinId;
    enum class PinKind : int;
}

namespace minEngine
{
    class MaterialEdGraphNode;

    /** Stable imgui-node-editor IDs via semantic keys + HashCombine + bidirectional maps. */
    class MaterialGraphIds
    {
    public:
        struct PinKey
        {
            MaterialEdGraphNode* Node = nullptr;
            ax::NodeEditor::PinKind Kind = {};
            int32_t Index = 0;

            bool operator==(const PinKey& other) const;
        };

        struct LinkKey
        {
            MaterialEdGraphNode* FromNode = nullptr;
            int32_t FromOutputIndex = 0;
            MaterialEdGraphNode* ToNode = nullptr;
            int32_t ToInputIndex = 0;

            bool operator==(const LinkKey& other) const;
        };

        static void Reset();

        static ax::NodeEditor::NodeId ToNodeId(MaterialEdGraphNode* node);
        static MaterialEdGraphNode* FromNodeId(ax::NodeEditor::NodeId id);

        static ax::NodeEditor::PinId ToPinId(const PinKey& key);
        static bool FromPinId(ax::NodeEditor::PinId id, PinKey& outKey);

        static ax::NodeEditor::PinId ToPinId(
            MaterialEdGraphNode* node,
            ax::NodeEditor::PinKind kind,
            int32_t pinIndex);

        static bool FromPinId(
            ax::NodeEditor::PinId id,
            MaterialEdGraphNode*& outNode,
            ax::NodeEditor::PinKind& outKind,
            int32_t& outPinIndex);

        static ax::NodeEditor::LinkId ToLinkId(const LinkKey& key);
        static bool FromLinkId(ax::NodeEditor::LinkId id, LinkKey& outKey);

        static ax::NodeEditor::LinkId ToLinkId(
            MaterialEdGraphNode* fromNode,
            int32_t fromOutputIndex,
            MaterialEdGraphNode* toNode,
            int32_t toInputIndex);

        static bool FromLinkId(
            ax::NodeEditor::LinkId id,
            MaterialEdGraphNode*& outFromNode,
            int32_t& outFromOutputIndex,
            MaterialEdGraphNode*& outToNode,
            int32_t& outToInputIndex);
    };
}
