#include "Render/RenderGraph/RenderPass.h"

#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RHI/RHICommandList.h"

namespace minEngine
{
    RenderPass::RenderPass(std::string name)
        : m_Name(std::move(name))
    {
    }

    void RenderPass::SetImplementation(std::unique_ptr<IRenderPass> implementation)
    {
        m_Implementation = std::move(implementation);
    }

    void RenderPass::SetSetup(std::function<void(RenderPassBuilder&)> callback)
    {
        m_SetupCallback = std::move(callback);
    }

    void RenderPass::SetPreparePass(std::function<void(RenderGraphFrameResources&)> callback)
    {
        m_PrepareCallback = std::move(callback);
    }

    void RenderPass::SetBuildRenderPass(std::function<void(RHICommandList&, const PassParameters&)> callback)
    {
        m_BuildCallback = std::move(callback);
    }

    void RenderPass::RunSetup(RenderGraph& graph)
    {
        if (m_SetupDone)
        {
            return;
        }

        RenderPassBuilder builder(graph, *this);
        if (m_Implementation)
        {
            m_Implementation->Setup(builder);
        }
        else if (m_SetupCallback)
        {
            m_SetupCallback(builder);
        }

        m_SetupDone = true;
    }

    void RenderPass::PreparePass(RenderGraphFrameResources& frameResources) const
    {
        if (m_Implementation)
        {
            m_Implementation->PreparePass(frameResources);
            return;
        }

        if (m_PrepareCallback)
        {
            m_PrepareCallback(frameResources);
        }
    }

    void RenderPass::BuildRenderPass(RHICommandList& cmdList, const PassParameters& parameters) const
    {
        if (m_Implementation)
        {
            m_Implementation->BuildRenderPass(cmdList, parameters);
            return;
        }

        if (m_BuildCallback)
        {
            m_BuildCallback(cmdList, parameters);
        }
    }

    void RenderPass::AddDeclaredAccess(PassResourceAccess access)
    {
        m_DeclaredAccesses.push_back(std::move(access));
    }
}
