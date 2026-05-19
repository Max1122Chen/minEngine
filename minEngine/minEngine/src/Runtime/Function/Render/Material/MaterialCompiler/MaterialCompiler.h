#pragma once
#include "Core.h"
#include "MaterialCompileTypes.h"

namespace minEngine
{
    class MaterialEdGraph;
    class MIRGraph;

    class IMaterialTranslator
    {
    public:
        virtual ~IMaterialTranslator() = default;
        virtual MaterialCompiledShader Translate(const MIRGraph& graph) = 0;
    };

    class MaterialCompiler
    {
    public:
        static MaterialCompiledShader Compile(
            const MaterialEdGraph& graph,
            IMaterialTranslator& translator,
            const MaterialCompileEnvironment& env = {});
    };
}
