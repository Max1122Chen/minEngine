#include "PresentPass.h"

#include "Render/RenderSystem.h"
#include "Render/EngineShaderUtils.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHIBinding.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"

#include "Runtime/Function/Render/OpenGL/OpenGLRHIResources.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"

namespace minEngine
{
    void PresentPass::Initialize()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }

        RHICommandList cmdList(rhi);

        if (!m_ScreenQuadVertexLayout)
        {
            m_ScreenQuadVertexLayout = cmdList.CreateVertexInputLayout({
                {"a_Position", VertexElementType::Float2, false},
                {"a_TexCoord", VertexElementType::Float2, false}
            });
        }

        if (RHIShaderRef screenQuadShader = EngineShaderUtils::CreateShaderFromFiles(
                *rhi,
                EngineShaderUtils::EngineShaderPath("Present.vert"),
                EngineShaderUtils::EngineShaderPath("Present.frag")))
        {
            m_ScreenQuadShader = std::move(screenQuadShader);
        }

        RHIGraphicsPSODesc psoDesc;
        psoDesc.VertexShader = m_ScreenQuadShader.get();
        psoDesc.PixelShader = m_ScreenQuadShader.get();
        psoDesc.VertexInputLayout = m_ScreenQuadVertexLayout.get();
        psoDesc.DepthStencilState.bDepthTestEnabled = false;
        psoDesc.DepthStencilState.bDepthWriteEnabled = false;
        psoDesc.BlendState.bBlendEnabled = false;
        m_PresentPipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);

        m_PresentBindingLayout = cmdList.CreateBindingLayout({
            {0, RHIBindingType::TextureSRV, 0, RHIGraphicsShaderStage::Pixel}
        });
    }

    void PresentPass::Execute()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }
        RHICommandList cmdList(rhi);
        Execute(cmdList);
    }

    void PresentPass::Render()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }
        RHICommandList cmdList(rhi);
        Render(cmdList);
    }

    void PresentPass::Execute(RHICommandList& cmdList)
    {
        Render(cmdList);
    }

    void PresentPass::Render(RHICommandList& cmdList)
    {
        if (!m_ScreenQuadVertexLayout || !m_ScreenQuadShader || !m_SceneColorTexture || !m_PresentPipelineState)
        {
            ME_CORE_ERROR("PresentPass resources are not ready");
            return;
        }

        if (!m_SceneColorTexture)
        {
            return;
        }

        RHITextureSRVDesc srvDesc;
        srvDesc.Texture = m_SceneColorTexture.get();
        m_SceneColorSRV = std::make_shared<OpenGLRHIShaderResourceView>(srvDesc);

        RHIBindingResource bindingResource;
        bindingResource.Type = RHIBindingType::TextureSRV;
        bindingResource.TextureSRV = m_SceneColorSRV.get();
        auto bindingSet = cmdList.CreateBindingSet(
            m_PresentBindingLayout.get(),
            {bindingResource});

        RHIRenderPassInfo presentPassInfo;
        cmdList.BeginRenderPass(presentPassInfo);

        const uint32_t width = m_SceneColorTexture->GetDesc().Width;
        const uint32_t height = m_SceneColorTexture->GetDesc().Height;
        cmdList.SetViewport(0, 0, width, height);
        cmdList.SetGraphicsPipelineState(m_PresentPipelineState.get());
        cmdList.SetBindingSet(EngineShaderBindings::kSetEnginePost, bindingSet.get());
        cmdList.SetVertexInputLayout(m_ScreenQuadVertexLayout.get());
        if (m_ScreenQuadVertexBuffer)
        {
            cmdList.SetVertexBuffer(m_ScreenQuadVertexBuffer.get());
        }
        cmdList.Draw(6, 0);

        cmdList.EndRenderPass();
    }
}
