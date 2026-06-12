#include "TranslucencyPass.h"

#include "Render/RenderGraph/RenderGraphFrameResources.h"
#include "Render/RenderGraph/RenderGraphScenePass.h"
#include "Render/RenderGraph/RenderPassBuilder.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPipeline.h"
#include "Runtime/Function/Render/RenderPipeline/SceneMeshDrawUtils.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"

#include <algorithm>

namespace minEngine
{
    void TranslucencyPass::Setup(RenderPassBuilder& builder)
    {
        RDGTextureDesc desc{};
        builder.AddTextureInput(kRDGDirShadowAtlas);
        builder.AddColorOutput(kRDGSceneColor, desc);
        builder.SetDepthStencilOutput(kRDGSceneDepth, desc);
    }

    void TranslucencyPass::PreparePass(RenderGraphFrameResources& frameResources)
    {
        m_ActiveFrameResources = &frameResources;
        SortDrawCommands();

        if (!pipeline)
        {
            m_DrawPackets.clear();
            return;
        }

        PrepareSceneMeshDrawPackets(
            *pipeline,
            frameResources.GetCommandList(),
            m_DrawCommands,
            MeshPassKind::Translucent,
            m_DrawPackets);
    }

    void TranslucencyPass::BuildRenderPass(RHICommandList& cmdList, const PassParameters& parameters)
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

        RHIRenderPassInfo passInfo = MakeSceneRenderPassInfo(colorTexture, depthTexture, false);
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, colorTexture->GetDesc().Width, colorTexture->GetDesc().Height);
        SubmitSceneMeshDrawPackets(*pipeline, cmdList, m_DrawCommands, m_DrawPackets);
        cmdList.EndRenderPass();

        m_ActiveFrameResources->SetLastKnownUsage(kRDGSceneColor, RDGTextureUsage::RenderTarget);
        m_ActiveFrameResources->SetLastKnownUsage(kRDGSceneDepth, RDGTextureUsage::DepthWrite);
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
