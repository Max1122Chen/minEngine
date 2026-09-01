#pragma once

#include "../MaterialCompileTypes.h"
#include "../MaterialShellAssemblerBase.h"

namespace minEngine
{
    class GLSLMaterialShellAssemblerImpl : public MaterialShellAssemblerBase
    {
    public:
        bool Assemble(MaterialCompileResult& compiled, const MaterialCompileEnvironment& env);

    private:
        struct TemplateSet
        {
            const char* VertexTemplateFile = nullptr;
            const char* FragmentTemplateFile = nullptr;
        };

        static bool ResolveTemplateSet(
            MaterialShadingModel shadingModel,
            TemplateSet& outSet,
            MaterialCompileResult& compiled);

        static std::string BuildMaterialParametersStructGlobal(int numTexCoords);
        static std::string BuildVertexTexCoordSetup(int numTexCoords);
        static std::string BuildFragmentTexCoordSetup(int numTexCoords);
        static std::string BuildFragmentMaterialInputsStructGlobal(
            MaterialShadingModel shadingModel,
            MaterialBlendMode blendMode);
        static std::string BuildFragmentMaskedClip(const MaterialCompileEnvironment& env);
        static std::string BuildVertexIoBlock(
            int numTexCoords,
            bool includeSceneLightingVaryings,
            bool usesTangentFrame);
        static std::string BuildFragmentInTexCoords(int numTexCoords);
        static std::string BuildFragmentLightingVaryings(int numTexCoords, bool usesTangentFrame);
        static std::string BuildFragmentWorldNormal(const MaterialCompileEnvironment& env);
        static std::string BuildFragmentSceneLighting(
            const std::filesystem::path& assetsRoot,
            const MaterialCompileEnvironment& env,
            MaterialShadingModel shadingModel,
            MaterialCompileResult& compiled);
    };
}
