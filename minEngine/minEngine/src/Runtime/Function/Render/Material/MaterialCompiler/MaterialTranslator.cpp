#include "MaterialTranslator.h"

#include "GLSL/GLSLMaterialTranslatorImpl.h"
#include "Render/Material/MaterialIR/MIRGraph.h"

namespace minEngine
{
    MaterialCompiledShader MaterialTranslator::Translate(const MIRGraph& graph, const MaterialCompileEnvironment& env)
    {
        switch (env.ShaderLanguage)
        {
        case MaterialShaderLanguage::GLSL:
        {
            GLSLMaterialTranslatorImpl impl;
            return impl.Translate(graph, env);
        }
        default:
        {
            MaterialCompiledShader result;
            result.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "Unsupported MaterialShaderLanguage for MIR translation.",
            });
            return result;
        }
        }
    }
}
