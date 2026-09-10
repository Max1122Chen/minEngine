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

    struct Node
    {
        NodeId Id = kInvalidNodeId;
        ImVec2 Pos{0.0f, 0.0f};
        ImVec2 Size{160.0f, 56.0f};
        std::string Title;
        std::string Subtitle;
    };

    struct Edge
    {
        EdgeId Id = kInvalidEdgeId;
        NodeId From = kInvalidNodeId;
        NodeId To = kInvalidNodeId;
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
    };

    struct EditEvent
    {
        EditKind Kind = EditKind::SelectionChanged;
        NodeId Node = kInvalidNodeId;
        EdgeId Edge = kInvalidEdgeId;
        NodeId From = kInvalidNodeId;
        NodeId To = kInvalidNodeId;
        ImVec2 Pos{0.0f, 0.0f};
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