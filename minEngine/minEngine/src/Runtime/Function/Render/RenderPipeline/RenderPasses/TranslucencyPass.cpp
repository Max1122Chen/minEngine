#include "TranslucencyPass.h"

#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderPass.h"
#include "Render/RenderGraph/SceneRenderPassUtils.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/RenderPipeline/ForwardRenderer.h"
#include "Runtime/Function/Render/RenderPipeline/SceneMeshDrawUtils.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"

#include <algorithm>

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

    void TranslucencyPass::SetupDependencies(RenderPass& self, RenderGraph& graph)
    {
        (void)graph;
        self.AddColorOutput(kRDGSceneColor, MakeSceneColorAttachment());
        self.SetDepthStencilOutput(kRDGSceneDepth, MakeSceneDepthAttachment());
    }

    void TranslucencyPass::Prepare(RenderGraph& graph)
    {
        SortDrawCommands();
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
            MeshPassKind::Translucent,
            m_DrawPackets);
    }

    void TranslucencyPass::BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph)
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

        RHIRenderPassInfo passInfo = MakeSceneRenderPassInfo(colorTexture, depthTexture, false);
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, colorTexture->GetDesc().Width, colorTexture->GetDesc().Height);
        SubmitSceneMeshDrawPackets(*pipeline, cmdList, m_DrawCommands, m_DrawPackets);
        cmdList.EndRenderPass();
    }

    void TranslucencyPass::Execute()
    {
        SortDrawCommands();
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }
        RHICommandList cmdList(rhi);
        Execute(cmdList);
    }

    void TranslucencyPass::Execute(RHICommandList& cmdList)
    {
        Render(cmdList);
    }

    void TranslucencyPass::Render(RHICommandList& cmdList)
    {
        if (!pipeline)
        {
            return;
        }

        std::vector<MeshDrawPacket> drawPackets;
        PrepareSceneMeshDrawPackets(*pipeline, cmdList, m_DrawCommands, MeshPassKind::Translucent, drawPackets);
        SubmitSceneMeshDrawPackets(*pipeline, cmdList, m_DrawCommands, drawPackets);
    }

    void TranslucencyPass::SortDrawCommands()
    {
        RenderCamera* mainCamera = m_SortCamera;
        if (!mainCamera)
        {
            return;
        }

        const Vector3 cameraPos = mainCamera->m_Position;

        std::sort(m_DrawCommands.begin(), m_DrawCommands.end(), [cameraPos](const MeshDrawCommand& a, const MeshDrawCommand& b)
            {
                const Vector3 deltaA = cameraPos - glm::vec3(a.m_ModelMatrix[3]);
                const Vector3 deltaB = cameraPos - glm::vec3(b.m_ModelMatrix[3]);

                const float distanceA = glm::dot(deltaA, deltaA);
                const float distanceB = glm::dot(deltaB, deltaB);
                return distanceA > distanceB;
            });
    }
}
