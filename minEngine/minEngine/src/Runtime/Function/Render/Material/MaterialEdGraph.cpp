#include "MaterialEdGraph.h"

#include "MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "MaterialPropertyUtil.h"

namespace minEngine
{
    MaterialGraphNodeDefInput* MaterialEdGraph::FindPropertyGraphInput(MaterialProperty property) const
    {
        MaterialPropertyInputDescription description;
        if (!GetMaterialPropertyInputDescription(property, description))
        {
            return nullptr;
        }

        for (const MaterialEdGraphNode& node : m_Nodes)
        {
            if (node.m_Definition == nullptr)
            {
                continue;
            }

            if (MaterialGraphNodeDefInput* input = node.m_Definition->FindInputByName(description.InputName))
            {
                return input;
            }
        }

        return nullptr;
    }

    bool MaterialEdGraph::ResolveMaterialPropertyInput(MaterialProperty property, MaterialPropertyInputDescription& inOutDescription) const
    {
        if (!GetMaterialPropertyInputDescription(property, inOutDescription))
        {
            return false;
        }

        inOutDescription.GraphInput = FindPropertyGraphInput(property);
        return true;
    }

    std::vector<MaterialGraphNodeDef*> MaterialEdGraph::GetMaterialOutputNodeDefs() const
    {
        std::vector<MaterialGraphNodeDef*> outputNodes;
        outputNodes.reserve(m_Nodes.size());

        for (const MaterialEdGraphNode& node : m_Nodes)
        {
            if (node.m_Definition != nullptr && node.m_Definition->IsMaterialOutputNode())
            {
                outputNodes.push_back(node.m_Definition);
            }
        }

        return outputNodes;
    }
}
