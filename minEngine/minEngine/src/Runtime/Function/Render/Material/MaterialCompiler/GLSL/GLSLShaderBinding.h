#pragma once

#include "Core.h"

#include <string>

namespace minEngine
{
    // Cross-stage symbol contract between MIR lowering and shader shell templates (identifiers only).
    static constexpr int kMaxMaterialTexCoords = 8;

    inline const char* GetMaterialParametersSymbol()
    {
        return "MaterialParameters";
    }

    inline const char* GetFragmentMaterialInputsSymbol()
    {
        return "FragmentMaterialInputs";
    }

    // GLSL expression fragments shared by translator impl and shell assembler impl.
    inline std::string GetGLSLMaterialParametersTexCoordAccess(int texCoordIndex)
    {
        return std::string(GetMaterialParametersSymbol()) + ".TexCoords[" + std::to_string(texCoordIndex) + "]";
    }

    inline std::string GetGLSLMaterialTexCoordVaryingName(int texCoordIndex)
    {
        return "v_MaterialTexCoord" + std::to_string(texCoordIndex);
    }

    int GetRequiredMaterialTexCoordCount(bool usesTexCoord0);
}
