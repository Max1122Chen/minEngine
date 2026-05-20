#pragma once
#include "Core.h"
#include "Function/EditorGraph/EditorGraph.h"
#include "MaterialEdGraphNode.h"
#include "MaterialTypes.h"

#include <memory>
#include <utility>
#include <vector>

namespace minEngine
{
    struct MaterialPropertyInputDescription;
    class MaterialGraphNodeDef;
    class MaterialGraphNodeDefInput;

    class MaterialEdGraph : public EditorGraph
    {
    public:
        std::vector<MaterialEdGraphNode> m_Nodes;

        template<typename TNodeDef, typename... Args>
        MaterialEdGraphNode& AddNode(Args&&... args)
        {
            m_Nodes.emplace_back();
            MaterialEdGraphNode& node = m_Nodes.back();
            node.SetDefinition(std::make_unique<TNodeDef>(std::forward<Args>(args)...));
            return node;
        }

        bool ConnectPins(
            MaterialEdGraphNode& fromNode,
            int32_t fromOutputIndex,
            MaterialEdGraphNode& toNode,
            int32_t toInputIndex);

        void DisconnectInput(MaterialEdGraphNode& toNode, int32_t toInputIndex);

        bool ConnectToMaterialProperty(
            MaterialEdGraphNode& fromNode,
            int32_t fromOutputIndex,
            MaterialEdGraphNode& outputNode,
            MaterialProperty property);

        MaterialGraphNodeDefInput* FindPropertyGraphInput(MaterialProperty property) const;
        bool ResolveMaterialPropertyInput(MaterialProperty property, MaterialPropertyInputDescription& inOutDescription) const;
        std::vector<MaterialGraphNodeDef*> GetMaterialOutputNodeDefs() const;
    };
}
