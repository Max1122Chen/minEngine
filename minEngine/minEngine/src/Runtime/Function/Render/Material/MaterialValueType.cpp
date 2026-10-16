#include "MaterialValueType.h"

#include "MaterialEdGraph.h"
#include "MaterialEdGraphNode.h"
#include "MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "MaterialIR/MaterialIRTypes.h"
#include "MaterialCapability.h"
#include "MaterialPropertyUtil.h"

#include <string>

namespace minEngine
{
    bool MaterialValueTypeUtil::AreCompatible(MaterialValueType from, MaterialValueType to)
    {
        if (from == nullptr || to == nullptr)
        {
            return false;
        }

        if (from->IsPoison() || to->IsPoison())
        {
            return false;
        }

        return from == to;
    }

    bool MaterialValueTypeUtil::AreConnectable(MaterialValueType from, MaterialValueType to)
    {
        if (from == nullptr && to == nullptr)
        {
            return true;
        }

        if (from == nullptr || to == nullptr)
        {
            return true;
        }

        return AreCompatible(from, to);
    }

    const char* MaterialValueTypeUtil::GetDisplayName(MaterialValueType type)
    {
        if (type == nullptr || type->IsPoison())
        {
            return "Unknown";
        }

        if (const MIRPrimitiveType* primitive = type->AsPrimitive())
        {
            if (primitive == MIRPrimitiveType::GetBool())
            {
                return "Bool";
            }
            if (primitive == MIRPrimitiveType::GetInt())
            {
                return "Int";
            }
            if (primitive == MIRPrimitiveType::GetFloat())
            {
                return "Float";
            }
            if (primitive == MIRPrimitiveType::GetFloat2())
            {
                return "Float2";
            }
            if (primitive == MIRPrimitiveType::GetFloat3())
            {
                return "Float3";
            }
            if (primitive == MIRPrimitiveType::GetFloat4())
            {
                return "Float4";
            }
        }

        if (const MIRObjectType* object = type->AsObject())
        {
            if (object == MIRObjectType::GetTexture2D())
            {
                return "Texture2D";
            }
        }

        return "Unknown";
    }

    MaterialValueType MaterialValueTypeUtil::GetMaterialPropertyValueType(MaterialProperty property)
    {
        return GetMaterialPropertyType(property);
    }

    bool MaterialValueTypeUtil::ValidateGraphPinConnections(
        const MaterialEdGraph& graph,
        MaterialShadingModel shadingModel,
        MaterialBlendMode blendMode,
        std::string* outError)
    {
        for (const std::shared_ptr<MaterialEdGraphNode>& toNodePtr : graph.m_Nodes)
        {
            if (!toNodePtr)
            {
                continue;
            }

            MaterialGraphNodeDef* toDef = toNodePtr->GetNodeDef();
            if (toDef == nullptr)
            {
                continue;
            }

            for (int32_t inputIndex = 0; MaterialGraphNodeDefInput* input = toDef->GetInput(inputIndex);
                 ++inputIndex)
            {
                if (!input->IsConnected() || input->NodeDef == nullptr)
                {
                    continue;
                }

                if (toDef->IsMaterialOutputNode())
                {
                    const MaterialPropertyPinVisibility visibility =
                        MaterialCapabilityUtil::GetMaterialOutputInputVisibility(
                            input->Name.c_str(),
                            shadingModel,
                            blendMode);

                    if (visibility != MaterialPropertyPinVisibility::Active)
                    {
                        if (outError != nullptr)
                        {
                            *outError = std::string("Connection to inactive material output pin '") +
                                input->Name + "' is not allowed.";
                        }
                        return false;
                    }
                }

                const MaterialEdGraphNode* fromNodePtr = graph.FindEdNodeByNodeDef(input->NodeDef);
                if (fromNodePtr == nullptr)
                {
                    if (outError != nullptr)
                    {
                        *outError = "Connected source node is missing from the graph.";
                    }
                    return false;
                }

                MaterialValueType fromType = input->NodeDef->GetOutputPinType(input->OutputIndex);
                MaterialValueType toType = toDef->GetInputPinType(inputIndex);
                if (!AreConnectable(fromType, toType))
                {
                    if (outError != nullptr)
                    {
                        *outError = std::string("Incompatible pin types on connection to '") +
                            input->Name + "' (from " + GetDisplayName(fromType) + " to " +
                            GetDisplayName(toType) + ").";
                    }
                    return false;
                }
            }
        }

        return true;
    }
}
