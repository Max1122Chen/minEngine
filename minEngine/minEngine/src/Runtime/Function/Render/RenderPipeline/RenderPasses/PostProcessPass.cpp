#include "PostProcessPass.h"

#include "Render/DrawCommands/MeshDrawPacket.h"
#include "Render/EngineShaderBindings.h"
#include "Render/EnginePassUniforms.h"
#include "Render/EngineShaderUtils.h"
#include "Render/RenderSystem.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHIShaderBinding.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"

namespace minEngine
{
    void PostProcessPass::Initialize()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi || !m_PostProcessShader)
        {
            return;
        }

        RHICommandList cmdList(rhi);

        RHIBufferCreateDesc paramsDesc;
        paramsDesc.Usage = RHIBufferUsage::Uniform;
        paramsDesc.ByteSize = sizeof(EnginePostParamsUBO);
        m_PostParamsUniformBuffer = cmdList.CreateBuffer(paramsDesc, nullptr);

        m_PostShaderBindingSetLayout = cmdList.CreateShaderBindingSetLayout({
            {EngineShaderBindings::kEnginePost_SceneColorSRV,
             RHIShaderBindingType::TextureSRV,
             EngineShaderBindings::kGL_EnginePostSceneColorUnit,
             RHIGraphicsShaderStage::Pixel},
            {EngineShaderBindings::kEnginePost_Params,
             RHIShaderBindingType::UniformBuffer,
             EngineShaderBindings::kGL_EnginePostParamsUBO,
             RHIGraphicsShaderStage::Pixel},
        });
        m_PostPipelineLayout = cmdList.CreatePipelineLayout({m_PostShaderBindingSetLayout.get()});

        RHIGraphicsPSODesc psoDesc;
        psoDesc.PipelineLayout = m_PostPipelineLayout.get();
        psoDesc.VertexShader = m_PostProcessShader.get();
        psoDesc.PixelShader = m_PostProcessShader.get();
        psoDesc.VertexInputLayout = m_ScreenQuadVertexLayout.get();
        psoDesc.DepthStencilState.bDepthTestEnabled = false;
        psoDesc.DepthStencilState.bDepthWriteEnabled = false;
        psoDesc.BlendState.bBlendEnabled = false;
        m_PostProcessPipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);
    }

    void PostProcessPass::Execute()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }
        RHICommandList cmdList(rhi);
        Execute(cmdList);
    }

    void PostProcessPass::Execute(RHICommandList& cmdList)
    {
        Render(cmdList);
    }

    void PostProcessPass::Render(RHICommandList& cmdList)
    {
        if (!m_ScreenQuadVertexLayout || !m_PostProcessShader || !m_SceneColorTexture || !m_PostProcessPipelineState ||
            !m_PostShaderBindingSetLayout || !m_PostParamsUniformBuffer)
        {
            ME_CORE_ERROR("PostProcessPass resources are not ready");
            return;
        }

        RHITexture* sceneColorTexture = m_SceneColorTexture.get();
        if (sceneColorTexture != m_CachedSceneColorTexture)
        {
            m_CachedSceneColorTexture = sceneColorTexture;
            m_SceneColorSRV = m_TextureViewCache.GetOrCreate(cmdList, sceneColorTexture);
            m_PostShaderBindingSet.reset();
        }

        if (!m_PostShaderBindingSet && m_SceneColorSRV)
        {
            m_PostShaderBindingSet = cmdList.CreateShaderBindingSet(
                m_PostShaderBindingSetLayout.get(),
                {
                    {RHIShaderBindingType::TextureSRV, nullptr, m_SceneColorSRV.get()},
                    {RHIShaderBindingType::UniformBuffer, m_PostParamsUniformBuffer.get(), nullptr},
                });
        }

        EnginePostParamsUBO params{};
        params.InvResolution[0] = 1.0f / static_cast<float>(m_SceneColorTexture->GetDesc().Width);
        params.InvResolution[1] = 1.0f / static_cast<float>(m_SceneColorTexture->GetDesc().Height);
        params.ReduceMin = 1.0f / 128.0f;
        params.ReduceMul = 1.0f / 8.0f;
        params.SpanMax = 8.0f;
        params.Strength = 0.3f;
        params.EdgeThreshold = 0.1f;
        m_PostParamsUniformBuffer->UpdateSubresource(&params, 0, sizeof(EnginePostParamsUBO));

        MeshDrawPacket packet;
        packet.PipelineState = m_PostProcessPipelineState;
        packet.ShaderBindingSets[EngineShaderBindings::kSetEnginePost] = m_PostShaderBindingSet.get();
        packet.VertexBuffer = m_ScreenQuadVertexBuffer.get();
        cmdList.SubmitMeshDrawPacket(packet);
    }
}
