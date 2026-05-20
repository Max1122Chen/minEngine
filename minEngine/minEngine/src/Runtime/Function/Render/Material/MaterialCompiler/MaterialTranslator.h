#pragma once

#include "MaterialCompileTypes.h"

namespace minEngine
{
    class MIRGraph;

    class MaterialTranslator
    {
    public:
        static MaterialCompileResult Translate(const MIRGraph& graph, const MaterialCompileEnvironment& env);
    };
}
