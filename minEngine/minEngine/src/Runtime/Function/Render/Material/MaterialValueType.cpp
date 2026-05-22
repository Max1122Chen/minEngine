#include "MaterialValueType.h"

#include "MaterialEdGraph.h"
#include "MaterialEdGraphNode.h"
#include "MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "MaterialIR/MaterialIRTypes.h"
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

    MaterialValueType MaterialValueTypeUtil::GetNodeOutputPinType(
        const MaterialGraphNodeDef* nodeDef,
        int32_t outputIndex)
    {
        if (nodeDef == nullptr || outputIndex < 0 || outputIndex >= nodeDef->GetOutputCount())
        {
            return nullptr;
        }

        if (dynamic_cast<const MaterialGraphNodeDef_Constant*>(nodeDef) != nullptr)
        {
            return MIRPrimitiveType::GetFloat();
        }

        if (const MaterialGraphNodeDef_Constant3* constant3 =
                dynamic_cast<const MaterialGraphNodeDef_Constant3*>(nodeDef))
        {
            if (outputIndex == 0)
            {
                return MIRPrimitiveType::GetFloat3();
            }
            if (outputIndex >= 1 && outputIndex <= 3)
            {
                return MIRPrimitiveType::GetFloat();
            }
            return nullptr;
        }

        if (dynamic_cast<const MaterialGraphNodeDef_MakeFloat3*>(nodeDef) != nullptr)
        {
            return MIRPrimitiveType::GetFloat3();
        }

        if (dynamic_cast<const MaterialGraphNodeDef_ScalarParameter*>(nodeDef) != nullptr)
        {
            return MIRPrimitiveType::GetFloat();
        }

        if (dynamic_cast<const MaterialGraphNodeDef_TextureCoordinate*>(nodeDef) != nullptr)
        {
            return MIRPrimitiveType::GetFloat2();
        }

        if (dynamic_cast<const MaterialGraphNodeDef_TextureObject*>(nodeDef) != nullptr)
        {
            return MIRObjectType::GetTexture2D();
        }

        if (const MaterialGraphNodeDef_TextureSample* textureSample =
                dynamic_cast<const MaterialGraphNodeDef_TextureSample*>(nodeDef))
        {
            if (outputIndex == 0)
            {
                return MIRPrimitiveType::GetFloat4();
            }
            if (outputIndex == 1)
            {
                return MIRPrimitiveType::GetFloat3();
            }
            return nullptr;
        }

        if (dynamic_cast<const MaterialGraphNodeDef_Multiply*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Add*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Subtract*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Divide*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Max*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Min*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Negative*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Not*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Select*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_IfThenElse*>(nodeDef) != nullptr)
        {
            return nullptr;
        }

        if (dynamic_cast<const MaterialGraphNodeDef_ComponentMask*>(nodeDef) != nullptr)
        {
            return MIRPrimitiveType::GetFloat();
        }

        if (dynamic_cast<const MaterialGraphNodeDef_MaterialOutput*>(nodeDef) != nullptr)
        {
            return MIRPrimitiveType::GetFloat();
        }

        return nullptr;
    }

    MaterialValueType MaterialValueTypeUtil::GetNodeInputPinType(
        const MaterialGraphNodeDef* nodeDef,
        int32_t inputIndex)
    {
        if (nodeDef == nullptr || inputIndex < 0 || inputIndex >= nodeDef->GetInputCount())
        {
            return nullptr;
        }

        const MaterialGraphNodeDefInput* input = nodeDef->GetInput(inputIndex);
        if (input == nullptr)
        {
            return nullptr;
        }

        if (dynamic_cast<const MaterialGraphNodeDef_MakeFloat3*>(nodeDef) != nullptr)
        {
            return MIRPrimitiveType::GetFloat();
        }

        if (dynamic_cast<const MaterialGraphNodeDef_TextureSample*>(nodeDef) != nullptr)
        {
            if (input->Name == "Texture")
            {
                return MIRObjectType::GetTexture2D();
            }
            if (input->Name == "UV")
            {
                return MIRPrimitiveType::GetFloat2();
            }
            return nullptr;
        }

        if (const MaterialGraphNodeDef_MaterialOutput* materialOutput =
                dynamic_cast<const MaterialGraphNodeDef_MaterialOutput*>(nodeDef))
        {
            for (int32_t propertyIndex = 0; propertyIndex < MaterialShadingPropertyCount; ++propertyIndex)
            {
                const MaterialProperty property = static_cast<MaterialProperty>(propertyIndex);
                MaterialPropertyInputDescription description;
                if (!GetMaterialPropertyInputDescription(property, description))
                {
                    continue;
                }

                if (input->Name == description.InputName)
                {
                    return description.ExpectedType;
                }
            }
            return nullptr;
        }

        if (dynamic_cast<const MaterialGraphNodeDef_ComponentMask*>(nodeDef) != nullptr)
        {
            return MIRPrimitiveType::GetFloat3();
        }

        if (dynamic_cast<const MaterialGraphNodeDef_Not*>(nodeDef) != nullptr)
        {
            return MIRPrimitiveType::GetBool();
        }

        if (dynamic_cast<const MaterialGraphNodeDef_Multiply*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Add*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Subtract*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Divide*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Max*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Min*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Negative*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_Select*>(nodeDef) != nullptr ||
            dynamic_cast<const MaterialGraphNodeDef_IfThenElse*>(nodeDef) != nullptr)
        {
            return nullptr;
        }

        return nullptr;
    }

    bool MaterialValueTypeUtil::ValidateGraphPinConnections(
        const MaterialEdGraph& graph,
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

                const MaterialEdGraphNode* fromNodePtr = graph.FindEdNodeByNodeDef(input->NodeDef);
                if (fromNodePtr == nullptr)
                {
                    if (outError != nullptr)
                    {
                        *outError = "Connected source node is missing from the graph.";
                    }
                    return false;
                }

                MaterialValueType fromType = GetNodeOutputPinType(
                    input->NodeDef,
                    input->OutputIndex);
                MaterialValueType toType = GetNodeInputPinType(toDef, inputIndex);
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
