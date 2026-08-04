#include "PresentPass.h"

#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderPass.h"
#include "Render/RenderSystem.h"
#include "Render/EngineShaderUtils.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHIShaderBinding.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"

#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"

namespace minEngine
{
    void PresentPass::SetInputTextureName(const char* inputName)
    {
        m_InputTextureName = inputName != nullptr ? inputName : kRDGSceneColor;
    }

    void PresentPass::SetupDependencies(RenderPass& self, RenderGraph& graph)
    {
        (void)graph;
        self.AddTextureInput(m_InputTextureName);
    }

    void PresentPass::Prepare(RenderGraph& graph)
    {
        RHICommandList* cmdList = graph.GetFrameContext().CommandList;
        m_InputTexture = graph.TryGetPhysicalTexture(graph.FindTextureResource(m_InputTextureName));
        if (cmdList == nullptr)
        {
            return;
        }
        PrepareDrawPacket(*cmdList, m_InputTexture);
    }

    void PresentPass::BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph)
    {
        (void)graph;

        if (!m_DrawPacket.PipelineState || !m_PresentShaderBindingSet || !m_InputTexture)
        {
            return;
        }

        RHIRenderPassInfo presentPassInfo;
        cmdList.BeginRenderPass(presentPassInfo);

        const uint32_t width = m_InputTexture->GetDesc().Width;
        const uint32_t height = m_InputTexture->GetDesc().Height;
        cmdList.SetViewport(0, 0, width, height);
        cmdList.SubmitMeshDrawPacket(m_DrawPacket);

        cmdList.EndRenderPass();
    }

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

        if (RHIShaderRef screenQuadShader = EngineShaderUtils::CreateShaderFromSpirvFiles(
                *rhi,
                EngineShaderUtils::EngineShaderPath("Present.vert"),
                EngineShaderUtils::EngineShaderPath("Present.frag")))
        {
            m_ScreenQuadShader = std::move(screenQuadShader);
        }

        m_PresentShaderBindingSetLayout = cmdList.CreateShaderBindingSetLayout({
            {0, RHIShaderBindingType::TextureSRV, 0, RHIGraphicsShaderStage::Pixel}
        });
        m_PresentPipelineLayout = cmdList.CreatePipelineLayout({m_PresentShaderBindingSetLayout.get()});

        RHIGraphicsPSODesc psoDesc;
        psoDesc.PipelineLayout = m_PresentPipelineLayout.get();
        psoDesc.VertexShader = m_ScreenQuadShader.get();
        psoDesc.PixelShader = m_ScreenQuadShader.get();
        psoDesc.VertexInputLayout = m_ScreenQuadVertexLayout.get();
        psoDesc.DepthStencilState.bDepthTestEnabled = false;
        psoDesc.DepthStencilState.bDepthWriteEnabled = false;
        psoDesc.BlendState.bBlendEnabled = false;
        m_PresentPipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);
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
        if (!m_DrawPacket.PipelineState || !m_PresentShaderBindingSet || !m_InputTexture)
        {
            ME_CORE_ERROR("PresentPass resources are not ready");
            return;
        }

        RHIRenderPassInfo presentPassInfo;
        cmdList.BeginRenderPass(presentPassInfo);

        const uint32_t width = m_InputTexture->GetDesc().Width;
        const uint32_t height = m_InputTexture->GetDesc().Height;
        cmdList.SetViewport(0, 0, width, height);
        cmdList.SubmitMeshDrawPacket(m_DrawPacket);

        cmdList.EndRenderPass();
    }

    void PresentPass::PrepareDrawPacket(RHICommandList& cmdList, RHITexture* inputTexture)
    {
        if (!m_ScreenQuadVertexLayout || !m_ScreenQuadShader || !inputTexture || !m_PresentPipelineState ||
            !m_PresentShaderBindingSetLayout)
        {
            m_DrawPacket = {};
            m_PresentShaderBindingSet.reset();
            return;
        }

        if (inputTexture != m_CachedInputTexture)
        {
            m_CachedInputTexture = inputTexture;
            m_InputSRV = m_TextureViewCache.GetOrCreate(cmdList, inputTexture);
            m_PresentShaderBindingSet.reset();
        }

        if (!m_PresentShaderBindingSet && m_InputSRV)
        {
            RHIShaderBinding shaderBinding;
            shaderBinding.Type = RHIShaderBindingType::TextureSRV;
            shaderBinding.TextureSRV = m_InputSRV.get();
            m_PresentShaderBindingSet =
                cmdList.CreateShaderBindingSet(m_PresentShaderBindingSetLayout.get(), {shaderBinding});
        }

        m_DrawPacket.PipelineState = m_PresentPipelineState;
        m_DrawPacket.ShaderBindingSets[EngineShaderBindings::kSetEnginePost] = m_PresentShaderBindingSet.get();
        m_DrawPacket.VertexBuffer = m_ScreenQuadVertexBuffer.get();
    }
}
