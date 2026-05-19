#pragma once

#include "Core.h"

#include <string>

namespace minEngine
{
    // UE FMaterialVertexParameters / FMaterialPixelParameters::TexCoords[] analogue.
    static constexpr int kMaxMaterialTexCoords = 8;

    inline const char* GetMaterialParametersSymbol()
    {
        return "MaterialParameters";
    }

    inline std::string GetMaterialParametersTexCoordAccess(int texCoordIndex)
    {
        return std::string(GetMaterialParametersSymbol()) + ".TexCoords[" + std::to_string(texCoordIndex) + "]";
    }

    inline std::string GetMaterialTexCoordVaryingName(int texCoordIndex)
    {
        return "v_MaterialTexCoord" + std::to_string(texCoordIndex);
    }

    int GetRequiredMaterialTexCoordCount(bool usesTexCoord0);
}
