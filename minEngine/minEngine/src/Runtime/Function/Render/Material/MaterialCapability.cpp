#include "MaterialCapability.h"

#include "../Material.h"
#include "MaterialEdGraph.h"
#include "MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "MaterialPropertyUtil.h"

namespace minEngine
{
    MaterialPropertyPinVisibility MaterialCapabilityUtil::GetPropertyPinVisibility(
        MaterialProperty property,
        MaterialShadingModel shadingModel,
        MaterialBlendMode blendMode)
    {
        switch (property)
        {
        case MP_Albedo:
        case MP_Emissive:
            break;
        case MP_Normal:
        case MP_AO:
        case MP_Metallic:
        case MP_Roughness:
            if (shadingModel == MaterialShadingModel::Unlit)
            {
                return MaterialPropertyPinVisibility::Hidden;
            }
            break;
        case MP_Opacity:
            if (blendMode == MaterialBlendMode::Opaque)
            {
                return MaterialPropertyPinVisibility::Disabled;
            }
            return MaterialPropertyPinVisibility::Active;
        case MP_WorldPositionOffset:
            return MaterialPropertyPinVisibility::Hidden;
        default:
            return MaterialPropertyPinVisibility::Hidden;
        }

        return MaterialPropertyPinVisibility::Active;
    }

    bool MaterialCapabilityUtil::IsPropertyRequiredAtCompile(
        MaterialProperty property,
        MaterialShadingModel /*shadingModel*/,
        MaterialBlendMode blendMode)
    {
        if (property == MP_Opacity
            && (blendMode == MaterialBlendMode::Masked || blendMode == MaterialBlendMode::Translucent))
        {
            return true;
        }

        return false;
    }

    bool MaterialCapabilityUtil::IsPropertyEmittedAtCompile(
        MaterialProperty property,
        MaterialShadingModel shadingModel,
        MaterialBlendMode blendMode)
    {
        return GetPropertyPinVisibility(property, shadingModel, blendMode) != MaterialPropertyPinVisibility::Hidden;
    }

    MaterialPropertyPinVisibility MaterialCapabilityUtil::GetMaterialOutputInputVisibility(
        const char* inputName,
        MaterialShadingModel shadingModel,
        MaterialBlendMode blendMode)
    {
        MaterialProperty property = MP_Albedo;
        if (!TryGetMaterialPropertyFromInputName(inputName, property))
        {
            return MaterialPropertyPinVisibility::Active;
        }

        return GetPropertyPinVisibility(property, shadingModel, blendMode);
    }

    std::vector<MaterialProperty> MaterialCapabilityUtil::GetFragmentPropertiesEmittedAtCompile(
        MaterialShadingModel shadingModel,
        MaterialBlendMode blendMode)
    {
        std::vector<MaterialProperty> properties;
        for (int propertyIndex = 0; propertyIndex < MaterialShadingPropertyCount; ++propertyIndex)
        {
            const MaterialProperty property = static_cast<MaterialProperty>(propertyIndex);
            if (!IsPropertyEmittedAtCompile(property, shadingModel, blendMode))
            {
                continue;
            }

            if (!MaterialPropertyEvaluatesInStage(property, Stage_Fragment))
            {
                continue;
            }

            properties.push_back(property);
        }

        return properties;
    }

    void MaterialCapabilityUtil::PruneInvalidMaterialOutputLinks(Material& material)
    {
        if (!material.m_Graph)
        {
            return;
        }

        MaterialEdGraph& graph = *material.m_Graph;
        const MaterialShadingModel shadingModel = material.m_ShadingModel;
        const MaterialBlendMode blendMode = material.m_BlendMode;

        for (const std::shared_ptr<MaterialEdGraphNode>& nodePtr : graph.m_Nodes)
        {
            if (!nodePtr)
            {
                continue;
            }

            MaterialGraphNodeDef* nodeDef = nodePtr->GetNodeDef();
            if (nodeDef == nullptr || !nodeDef->IsMaterialOutputNode())
            {
                continue;
            }

            for (int32_t inputIndex = 0; MaterialGraphNodeDefInput* input = nodeDef->GetInput(inputIndex);
                 ++inputIndex)
            {
                const MaterialPropertyPinVisibility visibility = GetMaterialOutputInputVisibility(
                    input->Name.c_str(),
                    shadingModel,
                    blendMode);

                if (visibility != MaterialPropertyPinVisibility::Active && input->IsConnected())
                {
                    graph.DisconnectInput(*nodePtr, inputIndex);
                }
            }
        }
    }
}
