#include "TaskFlowBuilder.h"

#include "Runtime/Core/Log/LogSystem.h"

#include <cstdint>
#include <queue>

namespace minEngine
{
    uint32_t TaskFlowBuilder::AddNode(NodeFn fn)
    {
        if (!fn)
        {
            ME_LOG(LogCore, Error, "TaskFlowBuilder::AddNode rejected: empty function.");
            return UINT32_MAX;
        }

        const uint32_t index = static_cast<uint32_t>(m_Nodes.size());
        Node node;
        node.Fn = std::move(fn);
        m_Nodes.push_back(std::move(node));
        return index;
    }

    void TaskFlowBuilder::Precede(uint32_t a, uint32_t b)
    {
        if (a >= m_Nodes.size() || b >= m_Nodes.size())
        {
            ME_LOG(LogCore, Error, "TaskFlowBuilder::Precede rejected: out of range (a={}, b={}, n={}).",
                a, b, m_Nodes.size());
            return;
        }
        if (a == b)
        {
            ME_LOG(LogCore, Error, "TaskFlowBuilder::Precede rejected: self-edge.");
            return;
        }
        m_Nodes[a].Successors.push_back(b);
    }

    void TaskFlowBuilder::Precede(uint32_t a, std::initializer_list<uint32_t> bs)
    {
        for (uint32_t b : bs)
        {
            Precede(a, b);
        }
    }

    bool TaskFlowBuilder::ContainsCycle() const
    {
        const size_t n = m_Nodes.size();
        std::vector<int> indegree(n, 0);
        for (const Node& node : m_Nodes)
        {
            for (uint32_t v : node.Successors)
            {
                if (v < n)
                {
                    ++indegree[v];
                }
            }
        }

        std::queue<uint32_t> ready;
        for (uint32_t i = 0; i < n; ++i)
        {
            if (indegree[i] == 0)
            {
                ready.push(i);
            }
        }

        size_t seen = 0;
        while (!ready.empty())
        {
            const uint32_t u = ready.front();
            ready.pop();
            ++seen;
            for (uint32_t v : m_Nodes[u].Successors)
            {
                if (v >= n)
                {
                    continue;
                }
                if (--indegree[v] == 0)
                {
                    ready.push(v);
                }
            }
        }
        return seen != n;
    }
}
