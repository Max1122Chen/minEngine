#pragma once

#include "MaterialCompileTypes.h"

namespace minEngine
{
    class MaterialShellAssembler
    {
    public:
        static bool Assemble(MaterialCompiledShader& compiled, const MaterialCompileEnvironment& env);
    };
}
