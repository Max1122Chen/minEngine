#include "GraphClearPass.h"

#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderPass.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHITexture.h"

namespace minEngine
{
    void GraphClearPass::SetupDependencies(RenderPass& self, RenderGraph& graph)
    {
        (void)graph;
        RDGAttachmentInfo color{};
        color.SizeClass = RDGSizeClass::SwapchainRelative;
        color.SizeX = 1.0f;
        color.SizeY = 1.0f;
        color.Format = TextureFormat::RGBA8;
        self.AddColorOutput(kRDGSceneColor, color);
    }

    bool GraphClearPass::GetClearColor(uint32_t attachment, float outRGBA[4]) const
    {
        if (attachment != 0 || outRGBA == nullptr)
        {
            return false;
        }

        // Distinctive slate-blue so Editor viewport proves S05 (not black / not Phase1 idle).
        outRGBA[0] = 0.20f;
        outRGBA[1] = 0.35f;
        outRGBA[2] = 0.55f;
        outRGBA[3] = 1.0f;
        return true;
    }

    void GraphClearPass::BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph)
    {
        RHITexture* color = graph.TryGetPhysicalTexture(&graph.GetTextureResource(kRDGSceneColor));
        if (color == nullptr)
        {
            return;
        }

        RHIRenderPassInfo passInfo(color, RHIRenderTargetActions::ClearStore);
        GetClearColor(0, passInfo.ClearValue.Color);
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, color->GetDesc().Width, color->GetDesc().Height);
        cmdList.EndRenderPass();
    }
}
