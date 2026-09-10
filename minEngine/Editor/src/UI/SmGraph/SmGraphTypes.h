#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <imgui.h>

namespace minEngine::SmGraph
{
    using NodeId = uint64_t;
    using EdgeId = uint64_t;

    constexpr NodeId kInvalidNodeId = 0;
    constexpr EdgeId kInvalidEdgeId = 0;

    /** Reserved view-only nodes (not state indices). */
    constexpr NodeId kEntryNodeId = 900001;
    constexpr NodeId kAnyStateNodeId = 900002;

    /** Reserved edges. */
    constexpr EdgeId kEntryEdgeId = 900101;
    constexpr EdgeId kAnyStateEdgeBase = 800000;
    /** Exclusive end of AnyState edge id range (Entry uses ids above this). */
    constexpr EdgeId kAnyStateEdgeEnd = 900000;

    enum class NodeKind : uint8_t
    {
        State = 0,
        Entry = 1,
        AnyState = 2,
    };

    enum class EdgeKind : uint8_t
    {
        Transition = 0,
        EntryDefault = 1,
        AnyState = 2,
    };

    struct Node
    {
        NodeId Id = kInvalidNodeId;
        NodeKind Kind = NodeKind::State;
        ImVec2 Pos{0.0f, 0.0f};
        ImVec2 Size{160.0f, 56.0f};
        std::string Title;
        std::string Subtitle;
        bool Deletable = true;
    };

    struct Edge
    {
        EdgeId Id = kInvalidEdgeId;
        EdgeKind Kind = EdgeKind::Transition;
        NodeId From = kInvalidNodeId;
        NodeId To = kInvalidNodeId;
        bool CanReverse = true;
    };

    enum class SelectionKind : uint8_t
    {
        None = 0,
        Node = 1,
        Edge = 2,
    };

    struct Selection
    {
        SelectionKind Kind = SelectionKind::None;
        NodeId Node = kInvalidNodeId;
        EdgeId Edge = kInvalidEdgeId;

        void Clear()
        {
            Kind = SelectionKind::None;
            Node = kInvalidNodeId;
            Edge = kInvalidEdgeId;
        }

        bool HasSelection() const { return Kind != SelectionKind::None; }

        bool Equals(const Selection& other) const
        {
            return Kind == other.Kind && Node == other.Node && Edge == other.Edge;
        }
    };

    enum class EditKind : uint8_t
    {
        SelectionChanged = 0,
        NodeMoved = 1,
        CreateEdgeRequested = 2,
        DeleteSelectionRequested = 3,
        AddNodeRequested = 4,
        DeleteEdgeRequested = 5,
        ReverseEdgeRequested = 6,
        RenameNodeRequested = 7,
    };

    struct EditEvent
    {
        EditKind Kind = EditKind::SelectionChanged;
        NodeId Node = kInvalidNodeId;
        EdgeId Edge = kInvalidEdgeId;
        NodeId From = kInvalidNodeId;
        NodeId To = kInvalidNodeId;
        ImVec2 Pos{0.0f, 0.0f};
        std::string Text;
    };

    class Document
    {
    public:
        std::vector<Node>& GetNodes() { return m_Nodes; }
        const std::vector<Node>& GetNodes() const { return m_Nodes; }

        std::vector<Edge>& GetEdges() { return m_Edges; }
        const std::vector<Edge>& GetEdges() const { return m_Edges; }

        Selection& GetSelection() { return m_Selection; }
        const Selection& GetSelection() const { return m_Selection; }

        void Clear()
        {
            m_Nodes.clear();
            m_Edges.clear();
            m_Selection.Clear();
        }

        Node* FindNode(NodeId id)
        {
            for (Node& node : m_Nodes)
            {
                if (node.Id == id)
                {
                    return &node;
                }
            }
            return nullptr;
        }

        const Node* FindNode(NodeId id) const
        {
            for (const Node& node : m_Nodes)
            {
                if (node.Id == id)
                {
                    return &node;
                }
            }
            return nullptr;
        }

        Edge* FindEdge(EdgeId id)
        {
            for (Edge& edge : m_Edges)
            {
                if (edge.Id == id)
                {
                    return &edge;
                }
            }
            return nullptr;
        }

        const Edge* FindEdge(EdgeId id) const
        {
            for (const Edge& edge : m_Edges)
            {
                if (edge.Id == id)
                {
                    return &edge;
                }
            }
            return nullptr;
        }

    private:
        std::vector<Node> m_Nodes;
        std::vector<Edge> m_Edges;
        Selection m_Selection;
    };
}
