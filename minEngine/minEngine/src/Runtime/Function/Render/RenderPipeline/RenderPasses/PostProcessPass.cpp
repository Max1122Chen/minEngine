#include "PostProcessPass.h"

#include "Render/EngineShaderBindings.h"
#include "Render/EnginePassUniforms.h"
#include "Render/EngineShaderUtils.h"
#include "Render/RenderSystem.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHIBinding.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"

#include "Runtime/Function/Render/OpenGL/OpenGLRHIResources.h"

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

        RHIGraphicsPSODesc psoDesc;
        psoDesc.VertexShader = m_PostProcessShader.get();
        psoDesc.PixelShader = m_PostProcessShader.get();
        psoDesc.VertexInputLayout = m_ScreenQuadVertexLayout.get();
        psoDesc.DepthStencilState.bDepthTestEnabled = false;
        psoDesc.DepthStencilState.bDepthWriteEnabled = false;
        psoDesc.BlendState.bBlendEnabled = false;
        m_PostProcessPipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);

        RHIBufferCreateDesc paramsDesc;
        paramsDesc.Usage = RHIBufferUsage::Uniform;
        paramsDesc.ByteSize = sizeof(EnginePostParamsUBO);
        m_PostParamsUniformBuffer = cmdList.CreateBuffer(paramsDesc, nullptr);

        m_PostBindingLayout = cmdList.CreateBindingLayout({
            {EngineShaderBindings::kEnginePost_SceneColorSRV,
             RHIBindingType::TextureSRV,
             EngineShaderBindings::kGL_EnginePostSceneColorUnit,
             RHIGraphicsShaderStage::Pixel},
            {EngineShaderBindings::kEnginePost_Params,
             RHIBindingType::UniformBuffer,
             EngineShaderBindings::kGL_EnginePostParamsUBO,
             RHIGraphicsShaderStage::Pixel},
        });
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
            !m_PostBindingLayout || !m_PostParamsUniformBuffer)
        {
            ME_CORE_ERROR("PostProcessPass resources are not ready");
            return;
        }

        RHITextureSRVDesc srvDesc;
        srvDesc.Texture = m_SceneColorTexture.get();
        m_SceneColorSRV = std::make_shared<OpenGLRHIShaderResourceView>(srvDesc);

        EnginePostParamsUBO params{};
        params.InvResolution[0] = 1.0f / static_cast<float>(m_SceneColorTexture->GetDesc().Width);
        params.InvResolution[1] = 1.0f / static_cast<float>(m_SceneColorTexture->GetDesc().Height);
        params.ReduceMin = 1.0f / 128.0f;
        params.ReduceMul = 1.0f / 8.0f;
        params.SpanMax = 8.0f;
        params.Strength = 0.3f;
        params.EdgeThreshold = 0.1f;
        m_PostParamsUniformBuffer->UpdateSubresource(&params, 0, sizeof(EnginePostParamsUBO));

        auto bindingSet = cmdList.CreateBindingSet(
            m_PostBindingLayout.get(),
            {
                {RHIBindingType::TextureSRV, nullptr, m_SceneColorSRV.get()},
                {RHIBindingType::UniformBuffer, m_PostParamsUniformBuffer.get(), nullptr},
            });

        const SubmitDrawBinding postBindings[] = {
            {EngineShaderBindings::kSetEnginePost, bindingSet.get()},
        };
        cmdList.SubmitDraw(
            m_PostProcessPipelineState.get(),
            postBindings,
            static_cast<uint32_t>(sizeof(postBindings) / sizeof(postBindings[0])),
            m_ScreenQuadVertexBuffer.get(),
            nullptr);
    }
}
