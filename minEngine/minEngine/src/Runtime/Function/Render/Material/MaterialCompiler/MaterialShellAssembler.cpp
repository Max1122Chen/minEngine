#include "MaterialShellAssembler.h"

#include "GLSL/GLSLMaterialShellAssemblerImpl.h"

namespace minEngine
{
    bool MaterialShellAssembler::Assemble(MaterialCompiledShader& compiled, const MaterialCompileEnvironment& env)
    {
        switch (env.ShaderLanguage)
        {
        case MaterialShaderLanguage::GLSL:
        {
            GLSLMaterialShellAssemblerImpl impl;
            return impl.Assemble(compiled, env);
        }
        default:
            compiled.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "Unsupported MaterialShaderLanguage for shell assembly.",
            });
            return false;
        }
    }
}
