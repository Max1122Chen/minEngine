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

        struct VertexAttributeUsage
        {
            bool UsesVertexColor = false;
            bool UsesCustom0 = false;
            bool UsesCustom1 = false;

            int DenseAttributeVaryingCount() const
            {
                return (UsesVertexColor ? 1 : 0) + (UsesCustom0 ? 1 : 0) + (UsesCustom1 ? 1 : 0);
            }

            bool Any() const
            {
                return UsesVertexColor || UsesCustom0 || UsesCustom1;
            }
        };

        static bool ResolveTemplateSet(
            MaterialShadingModel shadingModel,
            TemplateSet& outSet,
            MaterialCompileResult& compiled);

        static std::string BuildMaterialParametersStructGlobal(int numTexCoords);
        static std::string BuildVertexTexCoordSetup(int numTexCoords, const VertexAttributeUsage& usage);
        static std::string BuildFragmentTexCoordSetup(int numTexCoords);
        static std::string BuildFragmentMaterialInputsStructGlobal(
            MaterialShadingModel shadingModel,
            MaterialBlendMode blendMode);
        static std::string BuildFragmentMaskedClip(const MaterialCompileEnvironment& env);
        static std::string BuildVertexIoBlock(
            int numTexCoords,
            bool includeSceneLightingVaryings,
            bool usesTangentFrame,
            const VertexAttributeUsage& usage,
            bool skinned);
        static std::string BuildFragmentInTexCoords(int numTexCoords, const VertexAttributeUsage& usage);
        static std::string BuildFragmentLightingVaryings(
            int numTexCoords,
            const VertexAttributeUsage& usage,
            bool usesTangentFrame);
        static std::string BuildFragmentWorldNormal(const MaterialCompileEnvironment& env);
        static std::string BuildFragmentSceneLighting(
            const std::filesystem::path& assetsRoot,
            const MaterialCompileEnvironment& env,
            MaterialShadingModel shadingModel,
            MaterialCompileResult& compiled);
    };
}
