#pragma once

#include "Core.h"
#include "MaterialTypes.h"

namespace minEngine
{
    struct MIRValueType;
    class MaterialEdGraph;
    class MaterialEdGraphNode;
    class MaterialGraphNodeDef;

    using MaterialValueType = const MIRValueType*;

    class MaterialValueTypeUtil
    {
    public:
        static bool AreCompatible(MaterialValueType from, MaterialValueType to);

        // P0: nullptr means polymorphic / inferred (Multiply, Select, …); then only reject when both are known and differ.
        static bool AreConnectable(MaterialValueType from, MaterialValueType to);
        static const char* GetDisplayName(MaterialValueType type);

        static MaterialValueType GetMaterialPropertyValueType(MaterialProperty property);
        static MaterialValueType GetNodeOutputPinType(
            const MaterialGraphNodeDef* nodeDef,
            int32_t outputIndex);
        static MaterialValueType GetNodeInputPinType(
            const MaterialGraphNodeDef* nodeDef,
            int32_t inputIndex);

        static bool ValidateGraphPinConnections(const MaterialEdGraph& graph, std::string* outError);
    };
}
