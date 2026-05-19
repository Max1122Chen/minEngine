#pragma once
#include "Core.h"
#include "MaterialCompileTypes.h"

namespace minEngine
{
    class MaterialEdGraph;

    class MaterialCompiler
    {
    public:
        static MaterialCompiledShader Compile(const MaterialEdGraph& graph, const MaterialCompileEnvironment& env = {});
    };
}
