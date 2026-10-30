#include "SceneMeshDrawUtils.h"

#include "Runtime/Function/Render/EnginePipelineLayouts.h"
#include "Runtime/Function/Render/EngineSceneBindingSets.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"
#include "Runtime/Function/Render/RenderPipeline/ForwardRenderer.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"

#include <utility>

namespace minEngine
{
    void PrepareSceneMeshDrawPackets(
        ForwardRenderer& pipeline,
        RHICommandList& cmdList,
        const std::vector<MeshDrawCommand>& drawCommands,
        MeshPassKind passKind,
        std::vector<MeshDrawPacket>& outPackets)
    {
        const bool translucentPass = passKind == MeshPassKind::Translucent;
        outPackets.resize(drawCommands.size());

        for (size_t drawIndex = 0; drawIndex < drawCommands.size(); ++drawIndex)
        {
            const MeshDrawCommand& drawCommand = drawCommands[drawIndex];
            MeshDrawPacket& packet = outPackets[drawIndex];
            packet = {};

            // The material belongs to the frame that produced this command cell: a proxy removal
            // keeps the material of the proxy it removed alive through the two collections that
            // follow it, so this pointer outlives every draw command built from that proxy. A
            // command with no material at all is a proxy that draws nothing, not a dangling one.
            //
            // Everything read through it here comes from the snapshot the material published at the
            // end of its last game tick, plus the GPU state the proxy owns on this role.
            MaterialRenderProxy* materialProxy = drawCommand.m_MaterialProxy;
            if (!materialProxy || !materialProxy->IsCompiledForDraw() || !drawCommand.m_VertexInputLayout)
            {
                continue;
            }

            RHIShader* shader = drawCommand.m_bSkinned
                ? materialProxy->GetGPUShaderSkinned()
                : materialProxy->GetGPUShader();
            if (!shader)
            {
                continue;
            }

            packet.PipelineState = pipeline.GetPipelineLayouts().GetOrCreateSceneMeshGraphicsPipelineState(
                cmdList,
                pipeline.GetSceneBindings(),
                materialProxy->GetMaterialShaderBindingSetLayout(),
                shader,
                drawCommand.m_VertexInputLayout,
                passKind);
            packet.BindDrawCommand(drawCommand);
            // The material slot is deliberately left empty here. Its set is the one object in this
            // packet a producer rebuilds while the packet is alive: BindForDraw drops the set the
            // proxy built before and builds another. Filling the slot here meant the packet spent
            // the time between preparation and submission naming a set that may already be gone,
            // and the refresh at submission only rescued the draws that happened to be submitted in
            // the same frame. The slot is filled once, after the rebuild, below.
        }
    }

    void SubmitSceneMeshDrawPackets(
        ForwardRenderer& pipeline,
        RHICommandList& cmdList,
        const std::vector<MeshDrawCommand>& drawCommands,
        std::vector<MeshDrawPacket>& drawPackets)
    {
        if (drawCommands.size() != drawPackets.size())
        {
            return;
        }

        EngineSceneBindingSets& sceneBindings = pipeline.GetSceneBindings();

        for (size_t drawIndex = 0; drawIndex < drawPackets.size(); ++drawIndex)
        {
            MeshDrawPacket& packet = drawPackets[drawIndex];
            if (!packet.PipelineState)
            {
                continue;
            }

            const MeshDrawCommand& drawCommand = drawCommands[drawIndex];
            MaterialRenderProxy* materialProxy = drawCommand.m_MaterialProxy;
            if (!materialProxy || !drawCommand.m_VertexBuffer)
            {
                continue;
            }

            RHIShaderBindingSetRef set0 = sceneBindings.BindNextPerObjectModel(
                cmdList,
                drawCommand.m_ModelMatrix,
                drawCommand.m_BonePalette,
                drawCommand.m_BoneCount);
            if (!set0)
            {
                continue;
            }

            // Every slot is filled from a reference the packet now owns, so the packet and any
            // stream that has not been replayed yet keep these sets alive after their producers
            // stop holding them.
            packet.ShaderBindingSets[EngineShaderBindings::kSetSceneObject] = std::move(set0);
            // Pipeline layout always has set 1; leave it unbound and some drivers drop the draw.
            packet.ShaderBindingSets[EngineShaderBindings::kSetShadowIBL] = sceneBindings.GetSceneSet1();

            // BindForDraw rebuilds the material set when the material compiled again. Reading the
            // set after it returns is what makes the packet name the set that exists now, and the
            // only place the material slot is ever filled.
            materialProxy->BindForDraw(cmdList);
            packet.ShaderBindingSets[EngineShaderBindings::kSetMaterial] =
                materialProxy->GetMaterialShaderBindingSetRef();
            cmdList.SubmitMeshDrawPacket(packet);
        }
    }
}
