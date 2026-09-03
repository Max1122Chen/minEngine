#pragma once
#include "Core.h"
#include "MaterialCompileTypes.h"

namespace minEngine
{
    class Material;
    class MaterialEdGraph;

    class MaterialCompiler
    {
    public:
        static bool Compile(Material& target, const MaterialCompileContext& ctx = {});
        static bool CompileSkinnedVariant(Material& target, const MaterialCompileContext& ctx = {});

        static MaterialCompileResult CompileForDiagnostics(
            const MaterialEdGraph& graph,
            MaterialShadingModel shadingModel,
            MaterialBlendMode blendMode = MaterialBlendMode::Opaque,
            const MaterialCompileContext& ctx = {});

    private:
        static MaterialCompileEnvironment MakePipelineSettings(
            MaterialShadingModel shadingModel,
            MaterialBlendMode blendMode,
            const MaterialCompileContext& ctx,
            MeshDeformationMode deformationMode = MeshDeformationMode::Rigid);

        static MaterialCompileResult CompileGraphToResult(
            const MaterialEdGraph& graph,
            const MaterialCompileEnvironment& env);
    };
}
