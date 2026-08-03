#include "PostProcessPass.h"

#include "Render/DrawCommands/MeshDrawPacket.h"
#include "Render/EngineShaderBindings.h"
#include "Render/EnginePassUniforms.h"
#include "Render/EngineShaderUtils.h"
#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderPass.h"
#include "Render/RenderSystem.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHIShaderBinding.h"
#include "Render/RHI/RHITexture.h"

namespace minEngine
{
    void PostProcessPass::SetGraphTextureNames(const char* inputName, const char* outputName)
    {
        m_InputTextureName = inputName != nullptr ? inputName : kRDGSceneColor;
        m_OutputTextureName = outputName != nullptr ? outputName : kRDGPostBufferA;
    }

    void PostProcessPass::SetOutputDesc(uint32_t width, uint32_t height)
    {
        m_OutputWidth = width;
        m_OutputHeight = height;
    }

    void PostProcessPass::SetupDependencies(RenderPass& self, RenderGraph& graph)
    {
        (void)graph;
        self.AddTextureInput(m_InputTextureName);
        RDGAttachmentInfo output{};
        output.SizeClass = RDGSizeClass::SwapchainRelative;
        output.SizeX = 1.0f;
        output.SizeY = 1.0f;
        output.Format = TextureFormat::RGBA8;
        self.AddColorOutput(m_OutputTextureName, output);
    }

    void PostProcessPass::Prepare(RenderGraph& graph)
    {
        m_CanRender = false;
        RHICommandList* cmdList = graph.GetFrameContext().CommandList;
        RHITexture* inputTexture = graph.TryGetPhysicalTexture(graph.FindTextureResource(m_InputTextureName));
        m_OutputTexture = graph.TryGetPhysicalTexture(graph.FindTextureResource(m_OutputTextureName));
        if (cmdList == nullptr)
        {
            return;
        }
        if (m_Predecessor != nullptr && !m_Predecessor->NeedRenderPass())
        {
            // Do not overwrite SceneColor with an unproduced ping-pong buffer.
            return;
        }
        PrepareDrawPacket(*cmdList, inputTexture);
        m_CanRender = m_DrawPacket.PipelineState != nullptr && m_PostShaderBindingSet != nullptr
            && m_OutputTexture != nullptr;
    }

    void PostProcessPass::BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph)
    {
        (void)graph;

        if (!m_CanRender)
        {
            return;
        }

        RHIRenderPassInfo passInfo(m_OutputTexture, RHIRenderTargetActions::DontLoadStore);
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(
            0,
            0,
            m_OutputTexture->GetDesc().Width,
            m_OutputTexture->GetDesc().Height);
        cmdList.SubmitMeshDrawPacket(m_DrawPacket);
        cmdList.EndRenderPass();
    }

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
        if (!m_DrawPacket.PipelineState || !m_PostShaderBindingSet || !m_OutputTexture)
        {
            ME_CORE_ERROR("PostProcessPass resources are not ready");
            return;
        }

        RHIRenderPassInfo passInfo(m_OutputTexture, RHIRenderTargetActions::DontLoadStore);
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(
            0,
            0,
            m_OutputTexture->GetDesc().Width,
            m_OutputTexture->GetDesc().Height);
        cmdList.SubmitMeshDrawPacket(m_DrawPacket);
        cmdList.EndRenderPass();
    }

    void PostProcessPass::PrepareDrawPacket(RHICommandList& cmdList, RHITexture* inputTexture)
    {
        if (!m_ScreenQuadVertexLayout || !m_PostProcessShader || !inputTexture || !m_PostProcessPipelineState ||
            !m_PostShaderBindingSetLayout || !m_PostParamsUniformBuffer)
        {
            m_DrawPacket = {};
            m_PostShaderBindingSet.reset();
            return;
        }

        if (inputTexture != m_CachedInputTexture)
        {
            m_CachedInputTexture = inputTexture;
            m_InputSRV = m_TextureViewCache.GetOrCreate(cmdList, inputTexture);
            m_PostShaderBindingSet.reset();
        }

        if (!m_PostShaderBindingSet && m_InputSRV)
        {
            m_PostShaderBindingSet = cmdList.CreateShaderBindingSet(
                m_PostShaderBindingSetLayout.get(),
                {
                    {RHIShaderBindingType::TextureSRV, nullptr, m_InputSRV.get()},
                    {RHIShaderBindingType::UniformBuffer, m_PostParamsUniformBuffer.get(), nullptr},
                });
        }

        EnginePostParamsUBO params{};
        params.InvResolution[0] = 1.0f / static_cast<float>(inputTexture->GetDesc().Width);
        params.InvResolution[1] = 1.0f / static_cast<float>(inputTexture->GetDesc().Height);
        params.ReduceMin = 1.0f / 128.0f;
        params.ReduceMul = 1.0f / 8.0f;
        params.SpanMax = 8.0f;
        params.Strength = 0.3f;
        params.EdgeThreshold = 0.1f;
        m_PostParamsUniformBuffer->UpdateSubresource(&params, 0, sizeof(EnginePostParamsUBO));

        m_DrawPacket.PipelineState = m_PostProcessPipelineState;
        m_DrawPacket.ShaderBindingSets[EngineShaderBindings::kSetEnginePost] = m_PostShaderBindingSet.get();
        m_DrawPacket.VertexBuffer = m_ScreenQuadVertexBuffer.get();
    }
}
