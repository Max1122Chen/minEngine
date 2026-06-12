#include "ShadowGraphPass.h"

#include "Render/RenderGraph/RenderGraphFrameResources.h"
#include "Render/RenderGraph/RenderPassBuilder.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPasses/ShadowPass.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPipeline.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"

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
    }

    void ShadowGraphPass::ClearCommand()
    {
        m_HasCommand = false;
    }

    void ShadowGraphPass::Setup(RenderPassBuilder& builder)
    {
        if (m_DepthSlotName.empty())
        {
            return;
        }

        RDGTextureDesc desc{};
        desc.Width = kShadowMapResolution;
        desc.Height = kShadowMapResolution;
        builder.SetDepthStencilOutput(m_DepthSlotName.c_str(), desc);
    }

    void ShadowGraphPass::PreparePass(RenderGraphFrameResources& frameResources)
    {
        if (!m_HasCommand)
        {
            return;
        }

        m_ShadowPass.PrepareShadowPass(frameResources.GetCommandList());
    }

    RHITexture* ShadowGraphPass::GetShadowTexture() const
    {
        return m_HasCommand && m_Command.Handle.Texture ? m_Command.Handle.Texture.get() : nullptr;
    }

    void ShadowGraphPass::BuildRenderPass(RHICommandList& cmdList, const PassParameters& parameters)
    {
        (void)parameters;

        if (!m_HasCommand)
        {
            return;
        }

        m_ShadowPass.RenderSingleDrawCommand(cmdList, m_Command);
    }
}
