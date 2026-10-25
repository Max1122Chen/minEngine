#pragma once

#include "EngineAPI.h"

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <vector>

namespace minEngine
{
    /**
     * Builds a one-shot worker DAG for TaskSystem::SubmitFlow.
     * Move into SubmitFlow; do not mutate after submit.
     */
    class MINENGINE_API TaskFlowBuilder
    {
    public:
        using NodeFn = std::function<void()>;

        struct Node
        {
            NodeFn Fn;
            std::vector<uint32_t> Successors;
        };

        uint32_t AddNode(NodeFn fn);
        void Precede(uint32_t a, uint32_t b);
        void Precede(uint32_t a, std::initializer_list<uint32_t> bs);

        size_t GetNodeCount() const { return m_Nodes.size(); }
        bool ContainsCycle() const;

        /** Move nodes out for TaskSystem; builder becomes empty. */
        std::vector<Node> ReleaseNodes() { return std::move(m_Nodes); }

    private:
        std::vector<Node> m_Nodes;
    };
}
