#include "MaterialPropertyUtil.h"

#include <cstring>

#include "MaterialIR/MaterialIR.h"
#include "MaterialIR/MaterialIRTypes.h"

namespace minEngine
{
    const char* GetMaterialPropertyName(MaterialProperty property)
    {
        switch (property)
        {
        case MP_Albedo: return "Albedo";
        case MP_Normal: return "Normal";
        case MP_AO: return "AO";
        case MP_Metallic: return "Metallic";
        case MP_Roughness: return "Roughness";
        case MP_Emissive: return "Emissive";
        case MP_Opacity: return "Opacity";
        case MP_WorldPositionOffset: return "WorldPositionOffset";
        default: return "Unknown";
        }
    }

    float GetMaterialPropertyDefault(MaterialProperty property)
    {
        switch (property)
        {
        case MP_Albedo: return 0.0f;
        case MP_Normal: return 0.0f;
        case MP_AO: return 1.0f;
        case MP_Metallic: return 0.0f;
        case MP_Roughness: return 0.5f;
        case MP_Emissive: return 0.0f;
        case MP_Opacity: return 1.0f;
        case MP_WorldPositionOffset: return 0.0f;
        default: return 0.0f;
        }
    }

    const MIRPrimitiveType* GetMaterialPropertyType(MaterialProperty property)
    {
        switch (property)
        {
        case MP_Albedo:
        case MP_Normal:
        case MP_Emissive:
            return MIRPrimitiveType::GetFloat3();
        case MP_AO:
        case MP_Metallic:
        case MP_Roughness:
        case MP_Opacity:
            return MIRPrimitiveType::GetFloat();
        case MP_WorldPositionOffset:
            return MIRPrimitiveType::GetFloat3();
        default:
            return MIRPrimitiveType::GetFloat();
        }
    }

    bool MaterialPropertyEvaluatesInStage(MaterialProperty property, ShaderStage stage)
    {
        if (property == MP_WorldPositionOffset)
        {
            return stage == Stage_Vertex;
        }

        return stage != Stage_Vertex;
    }

    bool TryGetMaterialPropertyFromInputName(const char* inputName, MaterialProperty& outProperty)
    {
        if (inputName == nullptr)
        {
            return false;
        }

        for (int32_t propertyIndex = 0; propertyIndex < MaterialShadingPropertyCount; ++propertyIndex)
        {
            const MaterialProperty property = static_cast<MaterialProperty>(propertyIndex);
            if (std::strcmp(inputName, GetMaterialPropertyName(property)) == 0)
            {
                outProperty = property;
                return true;
            }
        }

        return false;
    }

    bool GetMaterialPropertyInputDescription(MaterialProperty property, MaterialPropertyInputDescription& outDescription)
    {
        if (property < 0 || property >= MaterialShadingPropertyCount)
        {
            return false;
        }

        outDescription.InputName = GetMaterialPropertyName(property);
        outDescription.ExpectedType = GetMaterialPropertyType(property);
        outDescription.GraphInput = nullptr;
        return outDescription.InputName != nullptr && outDescription.ExpectedType != nullptr;
    }
}
