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

    void ShadowGraphPass::SetPermanentGraphOutput(ShadowGraphPermanentOutput output)
    {
        m_PermanentOutput = std::move(output);
        if (m_PermanentOutput.IsSet && !m_PermanentOutput.DepthResourceName.empty())
        {
            m_DepthSlotName = m_PermanentOutput.DepthResourceName;
        }
    }

    void ShadowGraphPass::Configure(const ShadowDrawCommand& command)
    {
        m_Command = command;
        m_HasCommand = command.Handle.IsValid();
        if (m_HasCommand && !m_Command.GraphDepthResourceName.empty() && !m_PermanentOutput.IsSet)
        {
            m_DepthSlotName = m_Command.GraphDepthResourceName;
        }
    }

    void ShadowGraphPass::ClearCommand()
    {
        m_HasCommand = false;
        m_Command = {};
    }

    void ShadowGraphPass::BindGraphTexture(const RHITextureRef& texture)
    {
        m_Command.Handle.Texture = texture;
    }

    const ShadowDrawCommand* ShadowGraphPass::GetDrawCommand() const
    {
        return m_HasCommand ? &m_Command : nullptr;
    }

    void ShadowGraphPass::DeclareDepthOutput(
        RenderPass& self,
        const ShadowResourceHandle& handle,
        const std::string& depthName)
    {
        if (depthName.empty() || !handle.Resolution.IsValid())
        {
            return;
        }

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

        self.SetDepthStencilOutput(depthName, depth);
    }

    void ShadowGraphPass::SetupDependencies(RenderPass& self, RenderGraph& graph)
    {
        (void)graph;
        if (m_PermanentOutput.IsSet)
        {
            ShadowResourceHandle handle{};
            handle.ResourceType = m_PermanentOutput.ResourceType;
            handle.Resolution = m_PermanentOutput.Resolution;
            handle.LayerCount = m_PermanentOutput.LayerCount;
            handle.SlotIndex = 0;
            DeclareDepthOutput(self, handle, m_PermanentOutput.DepthResourceName);
            return;
        }

        if (!m_HasCommand || m_DepthSlotName.empty())
        {
            return;
        }

        DeclareDepthOutput(self, m_Command.Handle, m_DepthSlotName);
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
