#pragma once

#include "MaterialIR/MaterialIR.h"
#include "MaterialTypes.h"

namespace minEngine
{
    class MaterialGraphNodeDefInput;

    // UE FMaterialInputDescription analogue: pin identity + expected MIR type (graph input resolved separately).
    struct MaterialPropertyInputDescription
    {
        const char* InputName = nullptr;
        const MIRPrimitiveType* ExpectedType = nullptr;
        MaterialGraphNodeDefInput* GraphInput = nullptr;
    };

    const char* GetMaterialPropertyName(MaterialProperty property);
    float GetMaterialPropertyDefault(MaterialProperty property);
    const MIRPrimitiveType* GetMaterialPropertyType(MaterialProperty property);
    bool MaterialPropertyEvaluatesInStage(MaterialProperty property, ShaderStage stage);

    bool GetMaterialPropertyInputDescription(MaterialProperty property, MaterialPropertyInputDescription& outDescription);
}
