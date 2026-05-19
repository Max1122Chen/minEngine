#pragma once

#include "../MaterialCompileTypes.h"
#include "../MaterialShellAssemblerBase.h"

namespace minEngine
{
    class GLSLMaterialShellAssemblerImpl : public MaterialShellAssemblerBase
    {
    public:
        bool Assemble(MaterialCompiledShader& compiled, const MaterialCompileEnvironment& env);

    private:
        struct TemplateSet
        {
            const char* VertexTemplateFile = nullptr;
            const char* FragmentTemplateFile = nullptr;
        };

        static bool ResolveTemplateSet(
            MaterialShadingModel shadingModel,
            TemplateSet& outSet,
            MaterialCompiledShader& compiled);

        static std::string BuildMaterialParametersStructGlobal(int numTexCoords);
        static std::string BuildVertexTexCoordSetup(int numTexCoords);
        static std::string BuildFragmentTexCoordSetup(int numTexCoords);
        static std::string BuildFragmentMaterialInputsStructGlobal();
        static std::string BuildVertexIoBlock(int numTexCoords);
        static std::string BuildFragmentInTexCoords(int numTexCoords);
    };
}
