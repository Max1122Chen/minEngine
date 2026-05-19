#include "MaterialShaderAssembler.h"

#include "MaterialShadingModel.h"

namespace minEngine
{
    bool MaterialShaderAssembler::Assemble(MaterialCompiledShader& compiled, const MaterialCompileEnvironment& env)
    {
        if (!compiled.Succeeded)
        {
            return false;
        }

        if (env.ShadingMode == MaterialShadingMode::DefaultLit)
        {
            compiled.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "MaterialShadingMode::DefaultLit is not implemented yet.",
            });
            compiled.Succeeded = false;
            return false;
        }

        const IMaterialShadingModel& shadingModel = GetMaterialShadingModel(env.ShadingMode);
        if (!shadingModel.AssembleVertexShader(compiled))
        {
            compiled.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "Failed to assemble vertex shader.",
            });
            compiled.Succeeded = false;
            return false;
        }

        if (!shadingModel.AssembleFragmentShader(compiled))
        {
            compiled.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "Failed to assemble fragment shader.",
            });
            compiled.Succeeded = false;
            return false;
        }

        return true;
    }
}
