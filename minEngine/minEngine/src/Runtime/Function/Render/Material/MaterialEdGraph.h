#pragma once
#include "Core.h"
#include "Function/EditorGraph/EditorGraph.h"
#include "MaterialEdGraphNode.h"
#include "MaterialCompiler/MaterialCompileTypes.h"
#include "MaterialTypes.h"

#include <memory>
#include <string>
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

        bool CanConnectPins(
            const MaterialEdGraphNode& fromNode,
            int32_t fromOutputIndex,
            const MaterialEdGraphNode& toNode,
            int32_t toInputIndex,
            MaterialShadingModel shadingModel,
            MaterialBlendMode blendMode,
            std::string* outReason = nullptr) const;

        bool ConnectPins(
            MaterialEdGraphNode& fromNode,
            int32_t fromOutputIndex,
            MaterialEdGraphNode& toNode,
            int32_t toInputIndex,
            MaterialShadingModel shadingModel,
            MaterialBlendMode blendMode);

        void DisconnectInput(MaterialEdGraphNode& toNode, int32_t toInputIndex);

        bool ConnectToMaterialProperty(
            MaterialEdGraphNode& fromNode,
            int32_t fromOutputIndex,
            MaterialEdGraphNode& outputNode,
            MaterialProperty property,
            MaterialShadingModel shadingModel,
            MaterialBlendMode blendMode);

        MaterialGraphNodeDefInput* FindPropertyGraphInput(MaterialProperty property) const;
        bool ResolveMaterialPropertyInput(MaterialProperty property, MaterialPropertyInputDescription& inOutDescription) const;
        std::vector<MaterialGraphNodeDef*> GetMaterialOutputNodeDefs() const;

        MaterialEdGraphNode* FindEdNodeByNodeDef(const MaterialGraphNodeDef* nodeDef);
        const MaterialEdGraphNode* FindEdNodeByNodeDef(const MaterialGraphNodeDef* nodeDef) const;

        /** True if any graph input pin reads this node output (live downstream use). */
        bool IsNodeOutputConnected(const MaterialGraphNodeDef& nodeDef, int32_t outputIndex) const;

        bool RemoveNode(MaterialEdGraphNode& node);
    };
}

#include "Generated/Reflection/MaterialEdGraph.gen.h"
