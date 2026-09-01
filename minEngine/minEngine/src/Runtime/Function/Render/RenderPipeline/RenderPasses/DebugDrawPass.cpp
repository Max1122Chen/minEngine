#include "DebugDrawPass.h"

#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderPass.h"
#include "Render/RenderGraph/SceneRenderPassUtils.h"
#include "Runtime/Function/Debug/DebugDrawService.h"
#include "Runtime/Function/Debug/DebugDrawTypes.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/EngineShaderUtils.h"
#include "Runtime/Function/Render/RenderPipeline/ForwardRenderer.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIRenderPass.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"

namespace minEngine
{
    namespace
    {
        constexpr uint32_t kInitialVertexBufferBytes = 64u * 1024u;

        RDGAttachmentInfo MakeSceneColorAttachment()
        {
            RDGAttachmentInfo info{};
            info.SizeClass = RDGSizeClass::SwapchainRelative;
            info.SizeX = 1.0f;
            info.SizeY = 1.0f;
            info.Format = TextureFormat::RGBA8;
            return info;
        }

        RDGAttachmentInfo MakeSceneDepthAttachment()
        {
            RDGAttachmentInfo info{};
            info.SizeClass = RDGSizeClass::SwapchainRelative;
            info.SizeX = 1.0f;
            info.SizeY = 1.0f;
            info.Format = TextureFormat::DEPTH24STENCIL8;
            return info;
        }
    }

    void DebugDrawPass::Initialize()
    {
        Shutdown();

        RHI* rhi = RenderSystem::Get().GetRHI();
        if (rhi == nullptr)
        {
            return;
        }

        m_Shader = EngineShaderUtils::CreateShaderFromSpirvFiles(
            *rhi,
            EngineShaderUtils::EngineShaderPath("DebugDraw.vert"),
            EngineShaderUtils::EngineShaderPath("DebugDraw.frag"));
        if (m_Shader == nullptr || !m_Shader->IsValid())
        {
            ME_CORE_ERROR("DebugDrawPass: failed to load DebugDraw shader.");
            return;
        }

        RHICommandList cmdList(rhi);

        m_VertexLayout = cmdList.CreateVertexInputLayout({
            {"a_Position", VertexElementType::Float3, false},
            {"a_Color", VertexElementType::Float4, false},
        });

        m_BindingSetLayout = cmdList.CreateShaderBindingSetLayout({
            {EngineShaderBindings::kSet0_PerFrame,
             RHIShaderBindingType::UniformBuffer,
             EngineShaderBindings::kGL_PerFrameUBO,
             RHIGraphicsShaderStage::Vertex},
        });

        m_PipelineLayout = cmdList.CreatePipelineLayout({m_BindingSetLayout.get()});

        RHIGraphicsPSODesc psoDesc;
        psoDesc.PipelineLayout = m_PipelineLayout.get();
        psoDesc.VertexShader = m_Shader.get();
        psoDesc.PixelShader = m_Shader.get();
        psoDesc.VertexInputLayout = m_VertexLayout.get();
        psoDesc.PrimitiveType = RHIPrimitiveType::LineList;
        psoDesc.RasterizerState.bCullEnabled = false;
        psoDesc.RasterizerState.CullMode = RHICullMode::None;
        psoDesc.DepthStencilState.bDepthTestEnabled = true;
        psoDesc.DepthStencilState.bDepthWriteEnabled = false;
        psoDesc.DepthStencilState.DepthCompare = RHIDepthCompareFunc::Less;
        m_PipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);

        RHIBufferCreateDesc vbDesc;
        vbDesc.Usage = RHIBufferUsage::Vertex;
        vbDesc.ByteSize = kInitialVertexBufferBytes;
        vbDesc.Stride = static_cast<uint32_t>(sizeof(DebugVertex));
        vbDesc.ElementCount = vbDesc.ByteSize / vbDesc.Stride;
        m_VertexBuffer = cmdList.CreateBuffer(vbDesc);
        m_VertexBufferCapacityBytes = vbDesc.ByteSize;

        m_IsReady = m_VertexBuffer != nullptr && m_PipelineState != nullptr && m_BindingSetLayout != nullptr;
    }

    void DebugDrawPass::Shutdown()
    {
        m_ShaderBindingSet.reset();
        m_PipelineState.reset();
        m_PipelineLayout.reset();
        m_BindingSetLayout.reset();
        m_VertexLayout.reset();
        m_VertexBuffer.reset();
        m_Shader.reset();
        m_VertexBufferCapacityBytes = 0;
        m_DrawVertexCount = 0;
        m_ShouldRender = false;
        m_IsReady = false;
    }

    void DebugDrawPass::Execute()
    {
        (void)0;
    }

    void DebugDrawPass::SetupDependencies(RenderPass& self, RenderGraph& graph)
    {
        (void)graph;
        self.AddTextureInput(kRDGSceneColor);
        self.AddColorOutput(kRDGSceneColor, MakeSceneColorAttachment());
        self.SetDepthStencilOutput(kRDGSceneDepth, MakeSceneDepthAttachment());
    }

    void DebugDrawPass::Prepare(RenderGraph& graph)
    {
        m_ShouldRender = false;
        m_DrawVertexCount = 0;

        if (!m_IsReady || pipeline == nullptr)
        {
            return;
        }

        RHICommandList* cmdList = graph.GetFrameContext().CommandList;
        if (cmdList == nullptr)
        {
            return;
        }

        DebugDrawService& debugDrawService = DebugDrawService::Get();
        debugDrawService.BuildFrameGeometry();

        const std::vector<DebugVertex>& vertices =
            debugDrawService.GetVertices(EDebugDepthMode::Tested);
        m_DrawVertexCount = static_cast<uint32_t>(vertices.size());

        if (m_DrawVertexCount > 0)
        {
            const uint32_t byteSize = m_DrawVertexCount * static_cast<uint32_t>(sizeof(DebugVertex));
            EnsureVertexBufferCapacity(*cmdList, byteSize);
            if (m_VertexBuffer != nullptr)
            {
                m_VertexBuffer->UpdateSubresource(vertices.data(), 0, byteSize);
            }
        }

        debugDrawService.ClearFrameQueues();

        ForwardRenderer* forwardRenderer = pipeline;
        RHIBuffer* perFrameBuffer = forwardRenderer->GetPerFrameUniformBuffer();
        if (perFrameBuffer == nullptr)
        {
            m_DrawVertexCount = 0;
            return;
        }

        m_ShaderBindingSet = cmdList->CreateShaderBindingSet(
            m_BindingSetLayout.get(),
            {{RHIShaderBindingType::UniformBuffer, perFrameBuffer, nullptr}});

        m_ShouldRender = m_DrawVertexCount > 0 && m_VertexBuffer != nullptr && m_ShaderBindingSet != nullptr;
    }

    void DebugDrawPass::BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph)
    {
        if (!m_ShouldRender || m_PipelineState == nullptr)
        {
            return;
        }

        RHITexture* colorTexture = graph.TryGetPhysicalTexture(&graph.GetTextureResource(kRDGSceneColor));
        RHITexture* depthTexture = graph.TryGetPhysicalTexture(&graph.GetTextureResource(kRDGSceneDepth));
        if (colorTexture == nullptr || depthTexture == nullptr)
        {
            return;
        }

        RHIRenderPassInfo passInfo = MakeSceneRenderPassInfo(colorTexture, depthTexture, false);
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, colorTexture->GetDesc().Width, colorTexture->GetDesc().Height);

        cmdList.SetGraphicsPipelineState(m_PipelineState.get());
        cmdList.SetShaderBindingSet(0, m_ShaderBindingSet.get());
        cmdList.SetVertexBuffer(m_VertexBuffer.get());
        cmdList.Draw(m_DrawVertexCount, 0);

        cmdList.EndRenderPass();
    }

    void DebugDrawPass::EnsureVertexBufferCapacity(RHICommandList& cmdList, const uint32_t requiredByteSize)
    {
        if (requiredByteSize <= m_VertexBufferCapacityBytes && m_VertexBuffer != nullptr)
        {
            return;
        }

        uint32_t newCapacity = m_VertexBufferCapacityBytes > 0 ? m_VertexBufferCapacityBytes : kInitialVertexBufferBytes;
        while (newCapacity < requiredByteSize)
        {
            newCapacity *= 2u;
        }

        RHIBufferCreateDesc vbDesc;
        vbDesc.Usage = RHIBufferUsage::Vertex;
        vbDesc.ByteSize = newCapacity;
        vbDesc.Stride = static_cast<uint32_t>(sizeof(DebugVertex));
        vbDesc.ElementCount = vbDesc.ByteSize / vbDesc.Stride;
        m_VertexBuffer = cmdList.CreateBuffer(vbDesc);
        m_VertexBufferCapacityBytes = newCapacity;
    }
}
