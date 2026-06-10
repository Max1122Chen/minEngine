#include "GLSLMaterialShellAssemblerImpl.h"

#include "GLSLShaderBinding.h"

#include "../../MaterialCapability.h"
#include "../../MaterialPropertyUtil.h"
#include "Render/Material/MaterialIR/MaterialIRTypes.h"

#include <vector>

namespace minEngine
{
    namespace
    {
        bool ShadingModelUsesSceneLighting(MaterialShadingModel shadingModel)
        {
            return shadingModel == MaterialShadingModel::BlinnPhong
                || shadingModel == MaterialShadingModel::PBR;
        }
    }

    bool GLSLMaterialShellAssemblerImpl::ResolveTemplateSet(
        MaterialShadingModel shadingModel,
        TemplateSet& outSet,
        MaterialCompileResult& compiled)
    {
        switch (shadingModel)
        {
        case MaterialShadingModel::Unlit:
            outSet.VertexTemplateFile = "Unlit.vert.template";
            outSet.FragmentTemplateFile = "Unlit.frag.template";
            return true;
        case MaterialShadingModel::BlinnPhong:
            outSet.VertexTemplateFile = "BlinnPhong.vert.template";
            outSet.FragmentTemplateFile = "BlinnPhong.frag.template";
            return true;
        case MaterialShadingModel::PBR:
            outSet.VertexTemplateFile = "PBR.vert.template";
            outSet.FragmentTemplateFile = "PBR.frag.template";
            return true;
        default:
            compiled.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "Unknown MaterialShadingModel.",
            });
            return false;
        }
    }

    std::string GLSLMaterialShellAssemblerImpl::BuildMaterialParametersStructGlobal(int numTexCoords)
    {
        if (numTexCoords <= 0)
        {
            return {};
        }

        std::string declaration = "struct {\n";
        declaration += "    vec2 TexCoords[" + std::to_string(numTexCoords) + "];\n";
        declaration += "} ";
        declaration += GetMaterialParametersSymbol();
        declaration += ";\n\n";
        return declaration;
    }

    std::string GLSLMaterialShellAssemblerImpl::BuildVertexTexCoordSetup(int numTexCoords)
    {
        std::string setup;
        for (int texCoordIndex = 0; texCoordIndex < numTexCoords; ++texCoordIndex)
        {
            setup += "    ";
            setup += GetGLSLMaterialParametersTexCoordAccess(texCoordIndex);
            setup += " = a_TexCoord;\n";
            setup += "    ";
            setup += GetGLSLMaterialTexCoordVaryingName(texCoordIndex);
            setup += " = ";
            setup += GetGLSLMaterialParametersTexCoordAccess(texCoordIndex);
            setup += ";\n";
        }
        return setup;
    }

    std::string GLSLMaterialShellAssemblerImpl::BuildFragmentTexCoordSetup(int numTexCoords)
    {
        std::string setup;
        for (int texCoordIndex = 0; texCoordIndex < numTexCoords; ++texCoordIndex)
        {
            setup += "    ";
            setup += GetGLSLMaterialParametersTexCoordAccess(texCoordIndex);
            setup += " = ";
            setup += GetGLSLMaterialTexCoordVaryingName(texCoordIndex);
            setup += ";\n";
        }
        if (!setup.empty())
        {
            setup += "\n";
        }
        return setup;
    }

    std::string GLSLMaterialShellAssemblerImpl::BuildFragmentMaterialInputsStructGlobal(
        MaterialShadingModel shadingModel,
        MaterialBlendMode blendMode)
    {
        std::string declaration = "struct {\n";
        for (int propertyIndex = 0; propertyIndex < MaterialShadingPropertyCount; ++propertyIndex)
        {
            const MaterialProperty property = static_cast<MaterialProperty>(propertyIndex);
            if (!MaterialCapabilityUtil::IsPropertyEmittedAtCompile(property, shadingModel, blendMode))
            {
                continue;
            }

            if (!MaterialPropertyEvaluatesInStage(property, Stage_Fragment))
            {
                continue;
            }

            const MIRPrimitiveType* propertyType = GetMaterialPropertyType(property);
            if (propertyType == nullptr)
            {
                continue;
            }

            declaration += "    ";
            if (propertyType->IsVector())
            {
                declaration += "vec" + std::to_string(propertyType->NumRows) + " ";
            }
            else
            {
                declaration += "float ";
            }
            declaration += GetMaterialPropertyName(property);
            declaration += ";\n";
        }
        declaration += "} ";
        declaration += GetFragmentMaterialInputsSymbol();
        declaration += ";\n\n";
        return declaration;
    }

    std::string GLSLMaterialShellAssemblerImpl::BuildVertexIoBlock(
        int numTexCoords,
        bool includeSceneLightingVaryings,
        bool usesTangentFrame)
    {
        std::string ioBlock;
        ioBlock += "layout(location = 0) in vec3 a_Position;\n";
        if (numTexCoords > 0)
        {
            ioBlock += "layout(location = 1) in vec2 a_TexCoord;\n";
            for (int texCoordIndex = 0; texCoordIndex < numTexCoords; ++texCoordIndex)
            {
                ioBlock += "out vec2 ";
                ioBlock += GetGLSLMaterialTexCoordVaryingName(texCoordIndex);
                ioBlock += ";\n";
            }
        }

        if (includeSceneLightingVaryings)
        {
            const int normalLocation = (numTexCoords > 0) ? 2 : 1;
            ioBlock += "layout(location = " + std::to_string(normalLocation) + ") in vec3 a_Normal;\n";
            if (usesTangentFrame)
            {
                const int tangentLocation = normalLocation + 1;
                ioBlock += "layout(location = " + std::to_string(tangentLocation) + ") in vec4 a_Tangent;\n";
            }
            ioBlock += "out vec3 v_WorldFragPos;\n";
            ioBlock += "out vec3 v_WorldNormal;\n";
            if (usesTangentFrame)
            {
                ioBlock += "out vec3 v_WorldTangent;\n";
                ioBlock += "out float v_TangentSign;\n";
            }
            ioBlock += "out vec4 v_FragPosViewSpace;\n";
        }

        return ioBlock;
    }

    std::string GLSLMaterialShellAssemblerImpl::BuildFragmentLightingVaryings(bool usesTangentFrame)
    {
        std::string block = "in vec3 v_WorldFragPos;\n"
                            "in vec3 v_WorldNormal;\n";
        if (usesTangentFrame)
        {
            block += "in vec3 v_WorldTangent;\n"
                     "in float v_TangentSign;\n";
        }
        block += "in vec4 v_FragPosViewSpace;\n\n";
        return block;
    }

    std::string GLSLMaterialShellAssemblerImpl::BuildFragmentWorldNormal(const MaterialCompileEnvironment& env)
    {
        if (env.UsesTangentFrame)
        {
            return "    vec3 norm = BuildWorldNormalFromTangentSpace(\n"
                   "        v_WorldTangent,\n"
                   "        v_WorldNormal,\n"
                   "        v_TangentSign,\n"
                   "        FragmentMaterialInputs.Normal);\n";
        }

        return "    vec3 norm = normalize(FragmentMaterialInputs.Normal);\n";
    }

    std::string GLSLMaterialShellAssemblerImpl::BuildFragmentSceneLighting(
        const std::filesystem::path& assetsRoot,
        const MaterialCompileEnvironment& env,
        MaterialShadingModel shadingModel,
        MaterialCompileResult& compiled)
    {
        if (!ShadingModelUsesSceneLighting(shadingModel))
        {
            return {};
        }

        std::string sceneLighting;
        std::string shadowsInclude;
        if (!LoadIncludeFile(assetsRoot, env, "MaterialSceneShadows.glslinc", shadowsInclude, compiled))
        {
            return {};
        }

        sceneLighting += shadowsInclude;
        if (!sceneLighting.empty() && sceneLighting.back() != '\n')
        {
            sceneLighting += '\n';
        }
        sceneLighting += '\n';

        const char* lightingIncludeFile = shadingModel == MaterialShadingModel::PBR
            ? "MaterialPBR.glslinc"
            : "MaterialPhongLighting.glslinc";
        std::string lightingInclude;
        if (!LoadIncludeFile(assetsRoot, env, lightingIncludeFile, lightingInclude, compiled))
        {
            return {};
        }

        sceneLighting += lightingInclude;

        if (shadingModel == MaterialShadingModel::PBR)
        {
            std::string iblInclude;
            if (!LoadIncludeFile(assetsRoot, env, "MaterialIBL.glslinc", iblInclude, compiled))
            {
                return {};
            }

            sceneLighting += iblInclude;
            if (!sceneLighting.empty() && sceneLighting.back() != '\n')
            {
                sceneLighting += '\n';
            }
            sceneLighting += '\n';

            sceneLighting +=
                "// Engine IBL (Set1 bindings; see EngineShaderBindings.h).\n"
                "layout (binding = 4) uniform samplerCube u_EnvIrradianceMap;\n"
                "layout (binding = 5) uniform samplerCube u_EnvPrefilterMap;\n"
                "layout (binding = 6) uniform sampler2D u_EnvBrdfLUT;\n";
        }

        if (env.UsesTangentFrame)
        {
            std::string tangentFrameInclude;
            if (!LoadIncludeFile(assetsRoot, env, "MaterialTangentFrame.glslinc", tangentFrameInclude, compiled))
            {
                return {};
            }

            sceneLighting += tangentFrameInclude;
        }

        return sceneLighting;
    }

    std::string GLSLMaterialShellAssemblerImpl::BuildFragmentInTexCoords(int numTexCoords)
    {
        std::string block;
        for (int texCoordIndex = 0; texCoordIndex < numTexCoords; ++texCoordIndex)
        {
            block += "in vec2 ";
            block += GetGLSLMaterialTexCoordVaryingName(texCoordIndex);
            block += ";\n";
        }
        if (!block.empty())
        {
            block += "\n";
        }
        return block;
    }

    std::string GLSLMaterialShellAssemblerImpl::BuildFragmentMaskedClip(const MaterialCompileEnvironment& env)
    {
        if (env.BlendMode != MaterialBlendMode::Masked)
        {
            return {};
        }

        std::string clip = "    if (";
        clip += GetFragmentMaterialInputsSymbol();
        clip += ".Opacity < ";
        clip += std::to_string(kMaskedClipThreshold);
        clip += ")\n        discard;\n\n";
        return clip;
    }

    bool GLSLMaterialShellAssemblerImpl::Assemble(MaterialCompileResult& compiled, const MaterialCompileEnvironment& env)
    {
        TemplateSet templateSet;
        if (!ResolveTemplateSet(env.ShadingModel, templateSet, compiled))
        {
            return false;
        }

        const int numTexCoords = GetRequiredMaterialTexCoordCount(compiled.UsesTexCoord0);
        const std::filesystem::path assetsRoot = ResolveEngineDefaultAssetsRoot(env);

        std::string meshVertexUniforms;
        if (!LoadIncludeFile(assetsRoot, env, "MeshVertexUniforms.glslinc", meshVertexUniforms, compiled))
        {
            return false;
        }

        std::string meshVertexPosition;
        if (!LoadIncludeFile(assetsRoot, env, "MeshVertexPosition.glslinc", meshVertexPosition, compiled))
        {
            return false;
        }

        std::string meshVertexLightingVaryings;
        const bool includeSceneLightingVaryings = ShadingModelUsesSceneLighting(env.ShadingModel);
        if (includeSceneLightingVaryings)
        {
            const char* lightingVaryingsInclude = env.UsesTangentFrame
                ? "MeshVertexTangentFrameVaryings.glslinc"
                : "MeshVertexLightingVaryings.glslinc";
            if (!LoadIncludeFile(assetsRoot, env, lightingVaryingsInclude, meshVertexLightingVaryings, compiled))
            {
                return false;
            }
        }

        const MaterialStageSource& vertexStage = compiled.Stages[Stage_Vertex];
        const std::vector<std::pair<std::string, std::string>> vertexAnchors = {
            { "VERTEX_IO_BLOCK",
                BuildVertexIoBlock(numTexCoords, includeSceneLightingVaryings, env.UsesTangentFrame) },
            { "VERTEX_MATERIAL_PARAMETERS_STRUCT",
                (numTexCoords > 0) ? BuildMaterialParametersStructGlobal(numTexCoords) : std::string{} },
            { "VERTEX_UNIFORMS", meshVertexUniforms },
            { "VERTEX_TEXCOORD_SETUP",
                (numTexCoords > 0) ? BuildVertexTexCoordSetup(numTexCoords) : std::string{} },
            { "VERTEX_LIGHTING_VARYINGS", meshVertexLightingVaryings },
            { "VERTEX_STAGE_BODY", EnsureTrailingNewline(vertexStage.Body) },
            { "VERTEX_POSITION", meshVertexPosition },
        };

        if (!AssembleStageFromTemplate(
                assetsRoot, env, templateSet.VertexTemplateFile, vertexAnchors, compiled.FullVertexShader, compiled))
        {
            return false;
        }

        const MaterialStageSource& fragmentStage = compiled.Stages[Stage_Fragment];
        std::string preamble = fragmentStage.Preamble;
        if (!preamble.empty() && preamble.back() != '\n')
        {
            preamble += '\n';
        }
        if (!preamble.empty())
        {
            preamble += "\n";
        }

        const std::string fragmentSceneLighting =
            BuildFragmentSceneLighting(assetsRoot, env, env.ShadingModel, compiled);
        if (fragmentSceneLighting.empty() && includeSceneLightingVaryings)
        {
            return false;
        }

        std::vector<std::pair<std::string, std::string>> fragmentAnchors = {
            { "FRAGMENT_IN_TEXCOORDS", BuildFragmentInTexCoords(numTexCoords) },
            { "FRAGMENT_LIGHTING_VARYINGS",
                includeSceneLightingVaryings ? BuildFragmentLightingVaryings(env.UsesTangentFrame) : std::string{} },
            { "FRAGMENT_PREAMBLE", preamble },
            { "FRAGMENT_SCENE_LIGHTING", fragmentSceneLighting },
            { "FRAGMENT_MATERIAL_INPUTS_STRUCT",
                BuildFragmentMaterialInputsStructGlobal(env.ShadingModel, env.BlendMode) },
            { "FRAGMENT_MATERIAL_PARAMETERS_STRUCT",
                (numTexCoords > 0) ? BuildMaterialParametersStructGlobal(numTexCoords) : std::string{} },
            { "FRAGMENT_TEXCOORD_SETUP",
                (numTexCoords > 0) ? BuildFragmentTexCoordSetup(numTexCoords) : std::string{} },
            { "FRAGMENT_STAGE_BODY", EnsureTrailingNewline(fragmentStage.Body) },
            { "FRAGMENT_MASKED_CLIP", BuildFragmentMaskedClip(env) },
        };
        if (ShadingModelUsesSceneLighting(env.ShadingModel))
        {
            fragmentAnchors.push_back({ "FRAGMENT_WORLD_NORMAL", BuildFragmentWorldNormal(env) });
        }

        if (!AssembleStageFromTemplate(
                assetsRoot,
                env,
                templateSet.FragmentTemplateFile,
                fragmentAnchors,
                compiled.FullFragmentShader,
                compiled))
        {
            return false;
        }

        return true;
    }
}
