#include "BasePass.h"

#include "Render/RenderGraph/RenderGraphFrameResources.h"
#include "Render/RenderGraph/RenderGraphScenePass.h"
#include "Render/RenderGraph/RenderPassBuilder.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/RenderPipeline/ForwardRenderer.h"
#include "Runtime/Function/Render/RenderPipeline/SceneMeshDrawUtils.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"

namespace minEngine
{
    void BasePass::Setup(RenderPassBuilder& builder)
    {
        RDGTextureDesc desc{};
        builder.AddTextureInput(kRDGDirShadowAtlas);
        builder.AddColorOutput(kRDGSceneColor, desc);
        builder.SetDepthStencilOutput(kRDGSceneDepth, desc);
    }

    void BasePass::PreparePass(RenderGraphFrameResources& frameResources)
    {
        m_ActiveFrameResources = &frameResources;
        if (!pipeline)
        {
            m_DrawPackets.clear();
            return;
        }

        PrepareSceneMeshDrawPackets(
            *pipeline,
            frameResources.GetCommandList(),
            m_DrawCommands,
            MeshPassKind::Opaque,
            m_DrawPackets);
    }

    void BasePass::BuildRenderPass(RHICommandList& cmdList, const PassParameters& parameters)
    {
        (void)parameters;

        if (!pipeline || !m_ActiveFrameResources)
        {
            return;
        }

        RHITexture* colorTexture = m_ActiveFrameResources->GetRHI(kRDGSceneColor);
        RHITexture* depthTexture = m_ActiveFrameResources->GetRHI(kRDGSceneDepth);
        if (!colorTexture || !depthTexture)
        {
            return;
        }

        RHIRenderPassInfo passInfo = MakeSceneRenderPassInfo(colorTexture, depthTexture, m_ClearSceneTargets);
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, colorTexture->GetDesc().Width, colorTexture->GetDesc().Height);
        SubmitSceneMeshDrawPackets(*pipeline, cmdList, m_DrawCommands, m_DrawPackets);
        cmdList.EndRenderPass();

        m_ActiveFrameResources->SetLastKnownUsage(kRDGSceneColor, RDGTextureUsage::RenderTarget);
        m_ActiveFrameResources->SetLastKnownUsage(kRDGSceneDepth, RDGTextureUsage::DepthWrite);
    }

    void BasePass::Execute()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }
        RHICommandList cmdList(rhi);
        Execute(cmdList);
    }

    void BasePass::Execute(RHICommandList& cmdList)
    {
        Render(cmdList);
    }

    void BasePass::Render(RHICommandList& cmdList)
    {
        if (!pipeline)
        {
            return;
        }

        std::vector<MeshDrawPacket> drawPackets;
        PrepareSceneMeshDrawPackets(*pipeline, cmdList, m_DrawCommands, MeshPassKind::Opaque, drawPackets);
        SubmitSceneMeshDrawPackets(*pipeline, cmdList, m_DrawCommands, drawPackets);
    }
}
