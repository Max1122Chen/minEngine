#include "RenderPassBase.h"

#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/EnginePipelineLayouts.h"
#include "Runtime/Function/Render/EngineSceneBindingSets.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPipeline.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"

namespace minEngine
{
    void RenderPassBase::PrepareMeshDrawPackets(
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

            if (pipeline)
            {
                packet.PipelineState = pipeline->GetPipelineLayouts().GetOrCreateSceneMeshGraphicsPipelineState(
                    cmdList,
                    pipeline->GetSceneBindings(),
                    material->GetMaterialBindingLayout(),
                    shader,
                    drawCommand.m_VertexInputLayout,
                    translucentPass);
            }
            packet.VertexBuffer = drawCommand.m_VertexBuffer;
            packet.IndexBuffer = drawCommand.m_IndexBuffer;
            packet.BindingSets[EngineShaderBindings::kSetMaterial] = material->GetMaterialBindingSet();
        }
    }

    void RenderPassBase::SubmitSceneMeshDrawPackets(
        RHICommandList& cmdList,
        const std::vector<MeshDrawCommand>& drawCommands,
        std::vector<MeshDrawPacket>& drawPackets)
    {
        if (!pipeline || drawCommands.size() != drawPackets.size())
        {
            return;
        }

        const EngineSceneBindingSets& sceneBindings = pipeline->GetSceneBindings();

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

            sceneBindings.UpdatePerObjectModel(pipeline->GetPerObjectUniformBuffer(), drawCommand.m_ModelMatrix);
            packet.BindingSets[EngineShaderBindings::kSetSceneObject] = sceneBindings.GetSceneSet0();

            const bool bindSceneLighting = material->m_ShadingModel == MaterialShadingModel::BlinnPhong
                || material->m_ShadingModel == MaterialShadingModel::PBR;
            if (bindSceneLighting)
            {
                packet.BindingSets[EngineShaderBindings::kSetShadowIBL] = sceneBindings.GetSceneSet1();
            }

            material->BindForDraw(cmdList);
            cmdList.SubmitMeshDrawPacket(packet);
        }
    }
}

