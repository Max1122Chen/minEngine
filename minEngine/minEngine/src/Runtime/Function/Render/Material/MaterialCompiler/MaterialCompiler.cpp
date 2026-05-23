#include "MaterialCompiler.h"

#include "../../Material.h"
#include "MaterialShellAssembler.h"
#include "MaterialTranslator.h"
#include "../MaterialEdGraph.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "../MaterialPropertyUtil.h"
#include "../MaterialIR/MIRBuilder.h"
#include "../MaterialIR/MIRDebug.h"
#include "../MaterialIR/MIRGraph.h"

namespace minEngine
{
    MaterialCompileEnvironment MaterialCompiler::MakePipelineSettings(
        MaterialShadingModel shadingModel,
        MaterialBlendMode blendMode,
        const MaterialCompileContext& ctx)
    {
        MaterialCompileEnvironment env;
        env.ShadingModel = shadingModel;
        env.BlendMode = blendMode;
        env.ShaderLanguage = MaterialShaderLanguage::GLSL;
        env.UsesTangentFrame = shadingModel == MaterialShadingModel::BlinnPhong
            || shadingModel == MaterialShadingModel::PBR;
        env.EngineDefaultAssetsRootOverride = ctx.EngineDefaultAssetsRootOverride;
        return env;
    }

    static void AppendBlendModeOpacityWarnings(
        const MaterialEdGraph& graph,
        MaterialShadingModel shadingModel,
        MaterialBlendMode blendMode,
        MaterialCompileResult& result)
    {
        if (blendMode != MaterialBlendMode::Masked && blendMode != MaterialBlendMode::Translucent)
        {
            return;
        }

        MaterialPropertyInputDescription opacityInput;
        if (!graph.ResolveMaterialPropertyInput(MP_Opacity, opacityInput) ||
            opacityInput.GraphInput == nullptr || !opacityInput.GraphInput->IsConnected())
        {
            const char* blendLabel =
                (blendMode == MaterialBlendMode::Masked) ? "Masked" : "Translucent";
            result.Diagnostics.push_back({
                MaterialCompileDiagnostic::Warning,
                std::string(blendLabel)
                    + " material: Opacity is not connected; using default 1.0.",
            });
        }

        (void)shadingModel;
    }

    MaterialCompileResult MaterialCompiler::CompileGraphToResult(
        const MaterialEdGraph& graph,
        const MaterialCompileEnvironment& env)
    {
        const std::vector<MaterialGraphNodeDef*> materialOutputNodes = graph.GetMaterialOutputNodeDefs();
        if (materialOutputNodes.empty())
        {
            MaterialCompileResult result;
            result.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "Material graph requires at least one MaterialOutput node.",
            });
            return result;
        }

        MIRBuilder builder;
        for (MaterialGraphNodeDef* outputNodeDef : materialOutputNodes)
        {
            builder.AddRootNodeDef(outputNodeDef);
        }

        MIRGraph mirGraph;
        builder.Build(graph, mirGraph, env.ShadingModel, env.BlendMode);

        MaterialCompileResult compiled = MaterialTranslator::Translate(mirGraph, env);
        compiled.IRDump = DebugDumpMIR(mirGraph, "Material");
        AppendBlendModeOpacityWarnings(graph, env.ShadingModel, env.BlendMode, compiled);

        for (const std::string& diagnostic : mirGraph.GetDiagnostics())
        {
            compiled.Diagnostics.push_back({ MaterialCompileDiagnostic::Error, diagnostic });
        }

        compiled.Succeeded = compiled.Succeeded && mirGraph.IsValid();
        if (compiled.Succeeded)
        {
            compiled.Succeeded = MaterialShellAssembler::Assemble(compiled, env);
        }

        return compiled;
    }

    bool MaterialCompiler::Compile(Material& target, const MaterialCompileContext& ctx)
    {
        const MaterialCompileEnvironment env =
            MakePipelineSettings(target.m_ShadingModel, target.m_BlendMode, ctx);
        if (!target.m_Graph)
        {
            MaterialCompileResult failed;
            failed.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "Material has no editor graph.",
            });
            target.m_LastCompileDiagnostics = failed.Diagnostics;
            return false;
        }

        MaterialCompileResult result = CompileGraphToResult(*target.m_Graph, env);
        target.m_LastCompileDiagnostics = result.Diagnostics;

        if (!result.Succeeded)
        {
            return false;
        }

        return target.CommitCompileResult(result, ctx);
    }

    MaterialCompileResult MaterialCompiler::CompileForDiagnostics(
        const MaterialEdGraph& graph,
        MaterialShadingModel shadingModel,
        MaterialBlendMode blendMode,
        const MaterialCompileContext& ctx)
    {
        const MaterialCompileEnvironment env = MakePipelineSettings(shadingModel, blendMode, ctx);
        return CompileGraphToResult(graph, env);
    }
}
