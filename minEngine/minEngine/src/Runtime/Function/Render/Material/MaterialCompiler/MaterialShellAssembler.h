#pragma once

#include "MaterialCompileTypes.h"

namespace minEngine
{
    class MaterialShellAssembler
    {
    public:
        static bool Assemble(MaterialCompileResult& compiled, const MaterialCompileEnvironment& env);
    };
}
