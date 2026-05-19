#include "MaterialCompiler.h"

#include "MaterialShaderAssembler.h"
#include "../MaterialEdGraph.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "../MaterialIR/MIRBuilder.h"
#include "../MaterialIR/MIRDebug.h"
#include "../MaterialIR/MIRGraph.h"

namespace minEngine
{
    MaterialCompiledShader MaterialCompiler::Compile(
        const MaterialEdGraph& graph,
        IMaterialTranslator& translator,
        const MaterialCompileEnvironment& env)
    {
        const std::vector<MaterialGraphNodeDef*> materialOutputNodes = graph.GetMaterialOutputNodeDefs();
        if (materialOutputNodes.empty())
        {
            MaterialCompiledShader result;
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

        MaterialCompiledShader compiled = translator.Translate(mirGraph);
        compiled.IRDump = DebugDumpMIR(mirGraph, "Material");

        for (const std::string& diagnostic : mirGraph.GetDiagnostics())
        {
            compiled.Diagnostics.push_back({ MaterialCompileDiagnostic::Error, diagnostic });
        }

        compiled.Succeeded = compiled.Succeeded && mirGraph.IsValid();
        if (compiled.Succeeded)
        {
            compiled.Succeeded = MaterialShaderAssembler::Assemble(compiled, env);
        }

        return compiled;
    }
}
