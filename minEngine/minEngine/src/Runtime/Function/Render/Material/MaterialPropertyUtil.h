#pragma once

#include "Runtime/Core/EngineAPI.h"

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

    MINENGINE_API     MINENGINE_API const char* GetMaterialPropertyName(MaterialProperty property);
    MINENGINE_API float GetMaterialPropertyDefault(MaterialProperty property);
    MINENGINE_API const MIRPrimitiveType* GetMaterialPropertyType(MaterialProperty property);
    MINENGINE_API bool MaterialPropertyEvaluatesInStage(MaterialProperty property, ShaderStage stage);

    MINENGINE_API bool GetMaterialPropertyInputDescription(MaterialProperty property, MaterialPropertyInputDescription& outDescription);

    bool TryGetMaterialPropertyFromInputName(const char* inputName, MaterialProperty& outProperty);
}
