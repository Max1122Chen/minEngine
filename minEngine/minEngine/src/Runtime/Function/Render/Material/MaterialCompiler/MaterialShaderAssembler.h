#pragma once

#include "MaterialCompileTypes.h"

namespace minEngine
{
    class MaterialShaderAssembler
    {
    public:
        static bool Assemble(MaterialCompiledShader& compiled, const MaterialCompileEnvironment& env);
    };
}
