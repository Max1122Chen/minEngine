#pragma once
#include "Core.h"
#include "Function/EditorGraph/EditorGraph.h"
#include "MaterialEdGraphNode.h"
#include "MaterialTypes.h"

#include <memory>
#include <vector>

namespace minEngine::Reflection
{
    class MEClass;
}

namespace minEngine
{
    struct MaterialPropertyInputDescription;
    class Material;
    class MaterialGraphNodeDef;
    class MaterialGraphNodeDefInput;

    ME_CLASS()
    class MaterialEdGraph : public EditorGraph
    {
        ME_GENERATED_BODY(MaterialEdGraph)

    public:
        ME_PROPERTY(Instanced)
        std::vector<std::shared_ptr<MaterialEdGraphNode>> m_Nodes;

        MaterialEdGraphNode& AddNode(
            const Reflection::MEClass* nodeDefClass,
            float editorPosX = 0.0f,
            float editorPosY = 0.0f);

        template<typename TNodeDef>
        MaterialEdGraphNode& AddNode(float editorPosX = 0.0f, float editorPosY = 0.0f)
        {
            return AddNode(TNodeDef::StaticClass(), editorPosX, editorPosY);
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

        MaterialEdGraphNode* FindEdNodeByNodeDef(const MaterialGraphNodeDef* nodeDef);
        const MaterialEdGraphNode* FindEdNodeByNodeDef(const MaterialGraphNodeDef* nodeDef) const;

        bool RemoveNode(MaterialEdGraphNode& node);
    };
}

#include "Generated/Reflection/MaterialEdGraph.gen.h"
