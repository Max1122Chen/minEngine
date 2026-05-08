#pragma once
#include "Core.h"
#include "Function/EditorGraph/EditorGraph.h"
#include "MaterialEdGraphNode.h"
#include "MaterialTypes.h"

namespace minEngine
{
    class MaterialEdGraph : public EditorGraph
    {
    public:
        std::vector<MaterialEdGraphNode> m_Nodes;

        // Connect output pin (fromNodeIndex/fromOutputIndex) to input pin (toNodeIndex/toInputIndex)
        // This sets the underlying MaterialGraphNodeDef connection rather than manipulating NodeDef directly.
        bool ConnectNodes(int32_t fromNodeIndex, int32_t fromOutputIndex, int32_t toNodeIndex, int32_t toInputIndex)
        {
            if (fromNodeIndex < 0 || fromNodeIndex >= static_cast<int32_t>(m_Nodes.size())) return false;
            if (toNodeIndex < 0 || toNodeIndex >= static_cast<int32_t>(m_Nodes.size())) return false;

            MaterialEdGraphNode& fromNode = m_Nodes[fromNodeIndex];
            MaterialEdGraphNode& toNode = m_Nodes[toNodeIndex];

            if (!fromNode.m_Definition || !toNode.m_Definition) return false;

            if (fromNode.Pins.empty())
            {
                fromNode.CreateOutputPins();
            }

            if (toNode.Pins.empty())
            {
                toNode.CreateInputPins();
            }

            // validate indices
            if (fromOutputIndex < 0 || fromOutputIndex >= fromNode.m_Definition->GetOutputCount()) return false;
            if (toInputIndex < 0 || toInputIndex >= toNode.m_Definition->GetInputCount()) return false;

            EditorGraphNodePin* fromPin = fromNode.GetOutputPin(fromOutputIndex);
            EditorGraphNodePin* toPin = toNode.GetInputPin(toInputIndex);
            if (fromPin == nullptr || toPin == nullptr)
            {
                return false;
            }

            if (!fromPin->MakeLinkTo(toPin))
            {
                return false;
            }

            toNode.m_Definition->ConnectInput(toInputIndex, fromNode.m_Definition, fromOutputIndex);
            return true;
        }
    };
}