#pragma once

#include "../Material.h"
#include "Runtime/Core/Object/ObjectManager.h"

namespace minEngine
{
    template<typename TNodeDef>
    MaterialEdGraphNode& MaterialEdGraph::AddNode(Material& material)
    {
        if (!material.m_Graph)
        {
            material.m_Graph = NewObject<MaterialEdGraph>("", &material);
        }

        MaterialEdGraph* graphOuter = material.m_Graph.get();
        std::shared_ptr<MaterialEdGraphNode> edNode = NewObject<MaterialEdGraphNode>("", graphOuter);
        std::shared_ptr<MaterialGraphNodeDef> nodeDef = NewObject<TNodeDef>("", edNode.get());
        edNode->SetNodeDef(std::move(nodeDef));
        m_Nodes.push_back(std::move(edNode));
        return *m_Nodes.back();
    }
}
