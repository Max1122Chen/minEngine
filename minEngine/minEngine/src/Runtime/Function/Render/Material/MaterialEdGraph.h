#pragma once
#include "Core.h"
#include "Function/EditorGraph/EditorGraph.h"
#include "MaterialEdGraphNode.h"
#include "MaterialTypes.h"

namespace minEngine
{
    struct MaterialPropertyInputDescription;
    class MaterialGraphNodeDef;
    class MaterialGraphNodeDefInput;

    class MaterialEdGraph : public EditorGraph
    {
    public:
        std::vector<MaterialEdGraphNode> m_Nodes;

        MaterialGraphNodeDefInput* FindPropertyGraphInput(MaterialProperty property) const;
        bool ResolveMaterialPropertyInput(MaterialProperty property, MaterialPropertyInputDescription& inOutDescription) const;
        std::vector<MaterialGraphNodeDef*> GetMaterialOutputNodeDefs() const;
    };
}