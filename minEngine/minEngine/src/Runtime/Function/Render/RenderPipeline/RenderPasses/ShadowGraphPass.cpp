#include "ShadowGraphPass.h"

#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderPass.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPasses/ShadowPass.h"
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

    void ShadowGraphPass::SetupDependencies(RenderPass& self, RenderGraph& graph)
    {
        // Shadow maps remain manager-owned for S08; ForceIncludePass keeps this node in Bake.
        (void)self;
        (void)graph;
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
        if (!m_HasCommand)
        {
            return;
        }

        m_ShadowPass.RenderSingleDrawCommand(cmdList, m_Command);
    }

    bool ShadowGraphPass::NeedRenderPass() const
    {
        return m_HasCommand;
    }

    RHITexture* ShadowGraphPass::GetShadowTexture() const
    {
        return m_HasCommand && m_Command.Handle.Texture ? m_Command.Handle.Texture.get() : nullptr;
    }
}
