#include "BasePass.h"

#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderPass.h"
#include "Render/RenderGraph/SceneRenderPassUtils.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/RenderPipeline/ForwardRenderer.h"
#include "Runtime/Function/Render/RenderPipeline/SceneMeshDrawUtils.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"

namespace minEngine
{
    namespace
    {
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

    void BasePass::SetupDependencies(RenderPass& self, RenderGraph& graph)
    {
        (void)graph;
        self.AddColorOutput(kRDGSceneColor, MakeSceneColorAttachment());
        self.SetDepthStencilOutput(kRDGSceneDepth, MakeSceneDepthAttachment());
    }

    void BasePass::Prepare(RenderGraph& graph)
    {
        m_DrawPackets.clear();
        if (!pipeline)
        {
            return;
        }

        RHICommandList* cmdList = graph.GetFrameContext().CommandList;
        if (cmdList == nullptr)
        {
            return;
        }

        PrepareSceneMeshDrawPackets(
            *pipeline,
            *cmdList,
            m_DrawCommands,
            MeshPassKind::Opaque,
            m_DrawPackets);
    }

    void BasePass::BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph)
    {
        if (!pipeline)
        {
            return;
        }

        RHITexture* colorTexture = graph.TryGetPhysicalTexture(&graph.GetTextureResource(kRDGSceneColor));
        RHITexture* depthTexture = graph.TryGetPhysicalTexture(&graph.GetTextureResource(kRDGSceneDepth));
        if (colorTexture == nullptr || depthTexture == nullptr)
        {
            return;
        }

        RHIRenderPassInfo passInfo = MakeSceneRenderPassInfo(colorTexture, depthTexture, m_ClearSceneTargets);
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, colorTexture->GetDesc().Width, colorTexture->GetDesc().Height);

        SubmitSceneMeshDrawPackets(*pipeline, cmdList, m_DrawCommands, m_DrawPackets);
        cmdList.EndRenderPass();
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
