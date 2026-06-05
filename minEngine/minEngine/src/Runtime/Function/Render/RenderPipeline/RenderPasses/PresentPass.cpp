#include "PresentPass.h"

#include "Render/RenderSystem.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHIBinding.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"
#include "Render/Shader.h"

#include "Runtime/Function/Render/OpenGL/OpenGLRHIModern.h"
#include "Runtime/Function/Render/OpenGL/OpenGLShader.h"

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

        if (m_ScreenQuadVertexDefinition)
        {
            m_ScreenQuadVertexLayout = OpenGLRHIVertexInputLayout::WrapLegacyVertexDefinition(m_ScreenQuadVertexDefinition);
        }
        if (!m_ScreenQuadVertexLayout)
        {
            m_ScreenQuadVertexLayout = cmdList.CreateVertexInputLayout({
                {"a_Position", VertexElementType::Float2, false},
                {"a_TexCoord", VertexElementType::Float2, false}
            });
        }

        if (std::shared_ptr<Shader> screenQuadShader = Shader::CreateFromFiles(
                *rhi,
                Shader::EngineShaderPath("Present.vert"),
                Shader::EngineShaderPath("Present.frag")))
        {
            auto legacy = std::dynamic_pointer_cast<OpenGLShader>(screenQuadShader->GetRHIShader());
            if (legacy)
            {
                m_ScreenQuadShader = std::make_shared<OpenGLRHIShader>(legacy);
            }
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
            m_PresentBindingLayout.get(),
            {bindingResource});

        RHIRenderPassInfo presentPassInfo;
        cmdList.BeginRenderPass(presentPassInfo);

        const uint32_t width = m_SceneColorTexture->GetWidth();
        const uint32_t height = m_SceneColorTexture->GetHeight();
        cmdList.SetViewport(0, 0, width, height);
        cmdList.SetGraphicsPipelineState(m_PresentPipelineState.get());
        cmdList.SetBindingSet(0, bindingSet.get());
        if (auto* glShader = dynamic_cast<OpenGLRHIShader*>(m_ScreenQuadShader.get()))
        {
            if (OpenGLShader* legacyShader = glShader->GetGLShader())
            {
                legacyShader->UploadUniformInt("u_SceneColor", 0);
            }
        }
        cmdList.SetVertexInputLayout(m_ScreenQuadVertexLayout.get());
        if (auto modernVB = OpenGLRHIBuffer::WrapLegacyVertexBuffer(m_ScreenQuadVertexBuffer))
        {
            cmdList.SetVertexBuffer(modernVB.get());
        }
        cmdList.Draw(6, 0);

        cmdList.EndRenderPass();
    }
}
