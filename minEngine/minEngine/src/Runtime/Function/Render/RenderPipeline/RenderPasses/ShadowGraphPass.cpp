#include "ShadowGraphPass.h"

#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderPass.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPasses/ShadowPass.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"

#include <algorithm>

namespace minEngine
{
    ShadowGraphPass::ShadowGraphPass(ShadowPass& shadowPass)
        : m_ShadowPass(shadowPass)
    {
    }

    void ShadowGraphPass::SetSlotNames(std::string passName, std::string depthSlotName)
    {
        m_PassName = std::move(passName);
        m_DepthSlotName = std::move(depthSlotName);
    }

    void ShadowGraphPass::Configure(const ShadowDrawCommand& command)
    {
        m_Command = command;
        m_HasCommand = command.Handle.IsValid();
        if (m_HasCommand && !m_Command.GraphDepthResourceName.empty())
        {
            m_DepthSlotName = m_Command.GraphDepthResourceName;
        }
    }

    void ShadowGraphPass::ClearCommand()
    {
        m_HasCommand = false;
        m_Command = {};
    }

    void ShadowGraphPass::BindGraphTexture(RHITextureRef texture)
    {
        m_Command.Handle.Texture = std::move(texture);
    }

    void ShadowGraphPass::SetupDependencies(RenderPass& self, RenderGraph& graph)
    {
        (void)graph;
        if (!m_HasCommand || m_DepthSlotName.empty())
        {
            return;
        }

        const ShadowResourceHandle& handle = m_Command.Handle;
        RDGAttachmentInfo depth{};
        depth.SizeClass = RDGSizeClass::Absolute;
        depth.SizeX = static_cast<float>(handle.Resolution.Width);
        depth.SizeY = static_cast<float>(handle.Resolution.Height);
        depth.Format = TextureFormat::DEPTH32;

        switch (handle.ResourceType)
        {
        case ShadowResourceType::Depth2DArray:
            depth.Dimension = RHITextureDimension::Texture2DArray;
            depth.Layers = static_cast<uint32_t>(std::max(handle.LayerCount, 1));
            break;
        case ShadowResourceType::DepthCube:
            depth.Dimension = RHITextureDimension::TextureCube;
            depth.Layers = 6;
            break;
        case ShadowResourceType::Depth2D:
        default:
            depth.Dimension = RHITextureDimension::Texture2D;
            depth.Layers = 1;
            break;
        }

        self.SetDepthStencilOutput(m_DepthSlotName, depth);
    }

    void ShadowGraphPass::Prepare(RenderGraph& graph)
    {
        if (!m_HasCommand)
        {
            return;
        }

        RHICommandList* cmdList = graph.GetFrameContext().CommandList;
        if (cmdList == nullptr)
        {
            return;
        }

        m_ShadowPass.PrepareShadowPass(*cmdList);
    }

    void ShadowGraphPass::BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph)
    {
        (void)graph;
        if (!m_HasCommand || !m_Command.Handle.HasBoundTexture())
        {
            return;
        }

        m_ShadowPass.RenderSingleDrawCommand(cmdList, m_Command);
    }

    bool ShadowGraphPass::NeedRenderPass() const
    {
        return m_HasCommand && m_Command.Handle.HasBoundTexture();
    }

    RHITexture* ShadowGraphPass::GetShadowTexture() const
    {
        return m_HasCommand && m_Command.Handle.HasBoundTexture() ? m_Command.Handle.Texture.get() : nullptr;
    }
}
