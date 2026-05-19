#pragma once

#include "MaterialCompileTypes.h"

namespace minEngine
{
    class IMaterialShadingModel
    {
    public:
        virtual ~IMaterialShadingModel() = default;

        virtual bool AssembleVertexShader(MaterialCompiledShader& compiled) const = 0;
        virtual bool AssembleFragmentShader(MaterialCompiledShader& compiled) const = 0;
    };

    const IMaterialShadingModel& GetMaterialShadingModel(MaterialShadingMode shadingMode);
}
