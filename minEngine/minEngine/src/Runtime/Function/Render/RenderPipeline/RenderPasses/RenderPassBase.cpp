#include "RenderPassBase.h"

#include "Runtime/Function/Render/EngineSceneBindingSets.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPipeline.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"

namespace minEngine
{
    void RenderPassBase::PrepareMeshDrawCommands(
        RHICommandList& cmdList,
        std::vector<MeshDrawCommand>& drawCommands,
        MeshPassKind passKind)
    {
        const bool translucentPass = passKind == MeshPassKind::Translucent;

        for (MeshDrawCommand& drawCommand : drawCommands)
        {
            Material* material = drawCommand.m_Material;
            if (!material || !material->IsCompiledForDraw() || !drawCommand.m_VertexInputLayout)
            {
                drawCommand.m_PipelineState.reset();
                continue;
            }

            RHIShader* shader = material->GetGPUShader();
            if (!shader)
            {
                drawCommand.m_PipelineState.reset();
                continue;
            }

            RHIGraphicsPSODesc psoDesc;
            psoDesc.VertexShader = shader;
            psoDesc.PixelShader = shader;
            psoDesc.VertexInputLayout = drawCommand.m_VertexInputLayout;
            psoDesc.DepthStencilState.bDepthTestEnabled = true;
            psoDesc.DepthStencilState.DepthCompare = RHIDepthCompareFunc::Less;

            if (translucentPass)
            {
                psoDesc.BlendState.bBlendEnabled = true;
                psoDesc.DepthStencilState.bDepthWriteEnabled = false;
            }
            else
            {
                psoDesc.BlendState.bBlendEnabled = false;
                psoDesc.DepthStencilState.bDepthWriteEnabled = true;
            }

            drawCommand.m_PipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);
        }
    }

    void RenderPassBase::DrawMeshCommand(RHICommandList& cmdList, const MeshDrawCommand& drawCommand)
    {
        if (!drawCommand.m_PipelineState)
        {
            return;
        }

        cmdList.SubmitDrawMesh(drawCommand.m_PipelineState.get(), nullptr, 0, drawCommand);
    }

    void RenderPassBase::BindSceneDrawResources(
        RHICommandList& cmdList,
        const RenderPipeline& pipeline,
        const MeshPassSceneBinding& binding)
    {
        const EngineSceneBindingSets& sceneBindings = pipeline.GetSceneBindings();
        RHIBuffer* perObjectBuffer = pipeline.GetPerObjectUniformBuffer();
        sceneBindings.UpdatePerObjectModel(perObjectBuffer, binding.DrawCommand.m_ModelMatrix);

        cmdList.SetBindingSet(EngineShaderBindings::kSetSceneObject, sceneBindings.GetSceneSet0());

        if (!binding.bBindLighting)
        {
            return;
        }

        cmdList.SetBindingSet(EngineShaderBindings::kSetShadowIBL, sceneBindings.GetSceneSet1());
    }
}
