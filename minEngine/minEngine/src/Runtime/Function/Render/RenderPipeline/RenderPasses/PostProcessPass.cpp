#include "PostProcessPass.h"

#include "Render/RenderSystem.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHIBinding.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"
#include "Render/Shader.h"

#include "Runtime/Function/Render/OpenGL/OpenGLRHIModern.h"
#include "Runtime/Function/Render/OpenGL/OpenGLShader.h"

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

        if (m_ScreenQuadVertexDefinition)
        {
            m_ScreenQuadVertexLayout = OpenGLRHIVertexInputLayout::WrapLegacyVertexDefinition(m_ScreenQuadVertexDefinition);
        }

        if (auto glLegacy = std::dynamic_pointer_cast<OpenGLShader>(m_PostProcessShader))
        {
            m_PostProcessShaderRHI = std::make_shared<OpenGLRHIShader>(glLegacy);
        }

        RHIGraphicsPSODesc psoDesc;
        psoDesc.VertexShader = m_PostProcessShaderRHI.get();
        psoDesc.PixelShader = m_PostProcessShaderRHI.get();
        psoDesc.VertexInputLayout = m_ScreenQuadVertexLayout.get();
        psoDesc.DepthStencilState.bDepthTestEnabled = false;
        psoDesc.DepthStencilState.bDepthWriteEnabled = false;
        psoDesc.BlendState.bBlendEnabled = false;
        m_PostProcessPipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);

        m_SceneColorBindingLayout = cmdList.CreateBindingLayout({
            {0, RHIBindingType::TextureSRV, 0, RHIGraphicsShaderStage::Pixel}
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
        if (!m_ScreenQuadVertexLayout || !m_PostProcessShader || !m_SceneColorTexture || !m_PostProcessPipelineState)
        {
            ME_CORE_ERROR("PostProcessPass resources are not ready");
            return;
        }

        std::shared_ptr<RHITexture> sceneColorRHI = OpenGLRHITexture::WrapLegacy2D(m_SceneColorTexture);
        if (!sceneColorRHI)
        {
            return;
        }

        RHITextureSRVDesc srvDesc;
        srvDesc.Texture = sceneColorRHI.get();
        m_SceneColorSRV = std::make_shared<OpenGLRHIShaderResourceView>(srvDesc);

        RHIBindingResource bindingResource;
        bindingResource.Type = RHIBindingType::TextureSRV;
        bindingResource.TextureSRV = m_SceneColorSRV.get();
        auto bindingSet = cmdList.CreateBindingSet(
            m_SceneColorBindingLayout.get(),
            {bindingResource});

        cmdList.SetGraphicsPipelineState(m_PostProcessPipelineState.get());
        cmdList.SetBindingSet(0, bindingSet.get());

        m_PostProcessShader->Use();
        m_PostProcessShader->UploadUniformInt("u_SceneColor", 0);
        m_PostProcessShader->UploadUniformFloat2(
            "u_InvResolution",
            Vector2(1.0f / m_SceneColorTexture->GetWidth(), 1.0f / m_SceneColorTexture->GetHeight()));
        m_PostProcessShader->UploadUniformFloat("u_ReduceMin", 1.0f / 128.0f);
        m_PostProcessShader->UploadUniformFloat("u_ReduceMul", 1.0f / 8.0f);
        m_PostProcessShader->UploadUniformFloat("u_SpanMax", 8.0f);
        m_PostProcessShader->UploadUniformFloat("u_Strength", 0.3f);
        m_PostProcessShader->UploadUniformFloat("u_EdgeThreshold", 0.1f);

        cmdList.SetVertexInputLayout(m_ScreenQuadVertexLayout.get());
        if (auto modernVB = OpenGLRHIBuffer::WrapLegacyVertexBuffer(m_ScreenQuadVertexBuffer))
        {
            cmdList.SetVertexBuffer(modernVB.get());
        }
        cmdList.Draw(6, 0);
    }
}
