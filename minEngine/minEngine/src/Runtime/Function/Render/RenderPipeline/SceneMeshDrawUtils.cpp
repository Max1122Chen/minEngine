#include "SceneMeshDrawUtils.h"

#include "Runtime/Function/Render/EnginePipelineLayouts.h"
#include "Runtime/Function/Render/EngineSceneBindingSets.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"
#include "Runtime/Function/Render/RenderPipeline/ForwardRenderer.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"

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

            Material* material = drawCommand.m_Material;
            if (!material || !material->IsCompiledForDraw() || !drawCommand.m_VertexInputLayout)
            {
                continue;
            }

            RHIShader* shader = material->GetGPUShader();
            if (!shader)
            {
                continue;
            }

            packet.PipelineState = pipeline.GetPipelineLayouts().GetOrCreateSceneMeshGraphicsPipelineState(
                cmdList,
                pipeline.GetSceneBindings(),
                material->GetMaterialShaderBindingSetLayout(),
                shader,
                drawCommand.m_VertexInputLayout,
                translucentPass);
            packet.VertexBuffer = drawCommand.m_VertexBuffer;
            packet.IndexBuffer = drawCommand.m_IndexBuffer;
            packet.ShaderBindingSets[EngineShaderBindings::kSetMaterial] = material->GetMaterialShaderBindingSet();
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

        for (size_t drawIndex = 0; drawIndex < drawCommands.size(); ++drawIndex)
        {
            MeshDrawPacket& packet = drawPackets[drawIndex];
            if (!packet.PipelineState)
            {
                continue;
            }

            const MeshDrawCommand& drawCommand = drawCommands[drawIndex];
            Material* material = drawCommand.m_Material;
            if (!material || !drawCommand.m_VertexBuffer)
            {
                continue;
            }

            RHIShaderBindingSet* set0 = sceneBindings.BindNextPerObjectModel(cmdList, drawCommand.m_ModelMatrix);
            if (!set0)
            {
                continue;
            }

            packet.ShaderBindingSets[EngineShaderBindings::kSetSceneObject] = set0;
            // Pipeline layout always has set 1; leave it unbound and some drivers drop the draw.
            packet.ShaderBindingSets[EngineShaderBindings::kSetShadowIBL] = sceneBindings.GetSceneSet1();

            material->BindForDraw(cmdList);
            cmdList.SubmitMeshDrawPacket(packet);
        }
    }
}
