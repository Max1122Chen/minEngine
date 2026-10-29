#include "ScreenUIPass.h"

#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderPass.h"
#include "Render/RenderGraph/SceneRenderPassUtils.h"
#include "Runtime/Function/Render/EngineSceneBindingSets.h"
#include "Runtime/Function/Render/RenderPipeline/ForwardRenderer.h"
#include "Runtime/Function/Render/RenderPipeline/SceneMeshDrawUtils.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUICoords.h"

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

        /**
         * Points the per-object binder at the screen-space block while the UI draws are recorded.
         * Draws read their block at replay, so this only has to cover the recording of this pass.
         */
        class ScreenSpacePerFrameScope
        {
        public:
            ScreenSpacePerFrameScope(EngineSceneBindingSets& bindings, RHIBuffer* block)
                : m_Bindings(bindings)
            {
                m_Bindings.SetScreenSpacePerFrame(block);
            }

            ~ScreenSpacePerFrameScope()
            {
                m_Bindings.SetScreenSpacePerFrame(nullptr);
            }

            ScreenSpacePerFrameScope(const ScreenSpacePerFrameScope&) = delete;
            ScreenSpacePerFrameScope& operator=(const ScreenSpacePerFrameScope&) = delete;

        private:
            EngineSceneBindingSets& m_Bindings;
        };
    }

    void ScreenUIPass::SetupDependencies(RenderPass& self, RenderGraph& graph)
    {
        (void)graph;
        self.AddColorOutput(kRDGSceneColor, MakeSceneColorAttachment());
        self.SetDepthStencilOutput(kRDGSceneDepth, MakeSceneDepthAttachment());
    }

    void ScreenUIPass::BindIdentityCameraForScreenSpace()
    {
        if (m_ScreenSpacePerFrameUniformBuffer == nullptr || m_ViewportWidth == 0 || m_ViewportHeight == 0)
        {
            return;
        }

        const PerFrameData perFrameData = MakeScreenSpacePerFrameData(
            static_cast<float>(m_ViewportWidth),
            static_cast<float>(m_ViewportHeight));
        m_ScreenSpacePerFrameUniformBuffer->UpdateSubresource(&perFrameData, 0, sizeof(PerFrameData));
    }

    void ScreenUIPass::Prepare(RenderGraph& graph)
    {
        m_DrawPackets.clear();
        if (!pipeline || m_DrawCommands.empty())
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
            MeshPassKind::ScreenUI,
            m_DrawPackets);
    }

    void ScreenUIPass::BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph)
    {
        if (!pipeline || m_DrawCommands.empty())
        {
            return;
        }

        RHITexture* colorTexture = graph.TryGetPhysicalTexture(&graph.GetTextureResource(kRDGSceneColor));
        RHITexture* depthTexture = graph.TryGetPhysicalTexture(&graph.GetTextureResource(kRDGSceneDepth));
        if (colorTexture == nullptr || depthTexture == nullptr)
        {
            return;
        }

        BindIdentityCameraForScreenSpace();

        RHIRenderPassInfo passInfo = MakeSceneRenderPassInfo(colorTexture, depthTexture, false);
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, colorTexture->GetDesc().Width, colorTexture->GetDesc().Height);
        {
            ScreenSpacePerFrameScope screenSpaceScope(
                pipeline->GetSceneBindings(),
                m_ScreenSpacePerFrameUniformBuffer);
            SubmitSceneMeshDrawPackets(*pipeline, cmdList, m_DrawCommands, m_DrawPackets);
        }
        cmdList.EndRenderPass();
    }

    void ScreenUIPass::Execute()
    {
        // Graph path only.
    }
}
