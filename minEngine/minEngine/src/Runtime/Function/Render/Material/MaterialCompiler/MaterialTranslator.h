#pragma once

#include "MaterialCompileTypes.h"

namespace minEngine
{
    class MIRGraph;

    class MaterialTranslator
    {
    public:
        static MaterialCompiledShader Translate(const MIRGraph& graph, const MaterialCompileEnvironment& env);
    };
}
