#include "MaterialCompiler.h"

#include "../../Material.h"
#include "MaterialShellAssembler.h"
#include "MaterialTranslator.h"
#include "../MaterialEdGraph.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "../MaterialIR/MIRBuilder.h"
#include "../MaterialIR/MIRDebug.h"
#include "../MaterialIR/MIRGraph.h"

namespace minEngine
{
    MaterialCompileEnvironment MaterialCompiler::MakePipelineSettings(
        MaterialShadingModel shadingModel,
        const MaterialCompileContext& ctx)
    {
        MaterialCompileEnvironment env;
        env.ShadingModel = shadingModel;
        env.ShaderLanguage = MaterialShaderLanguage::GLSL;
        env.EngineDefaultAssetsRootOverride = ctx.EngineDefaultAssetsRootOverride;
        return env;
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
        builder.Build(graph, mirGraph);

        MaterialCompileResult compiled = MaterialTranslator::Translate(mirGraph, env);
        compiled.IRDump = DebugDumpMIR(mirGraph, "Material");

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
        const MaterialCompileEnvironment env = MakePipelineSettings(target.m_ShadingModel, ctx);
        MaterialCompileResult result = CompileGraphToResult(target.m_Graph, env);
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
        const MaterialCompileContext& ctx)
    {
        const MaterialCompileEnvironment env = MakePipelineSettings(shadingModel, ctx);
        return CompileGraphToResult(graph, env);
    }
}
