#include "Render/RenderGraph/RenderPass.h"

#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RHI/RHICommandList.h"

#include <stdexcept>
#include <utility>

namespace minEngine
{
    RenderPass::RenderPass(RenderGraph& graph, uint32_t index, std::string name, RDGQueue queue)
        : m_Graph(graph)
        , m_Index(index)
        , m_Name(std::move(name))
        , m_Queue(queue)
    {
    }

    RDGTextureResource& RenderPass::SetDepthStencilOutput(const std::string& name, const RDGAttachmentInfo& info)
    {
        RDGTextureResource& resource = m_Graph.GetOrCreateTextureResource(name);
        resource.SetAttachmentInfo(info);
        resource.AddUsage(RHITextureCreateFlags::RenderTarget);
        resource.MarkWrittenInPass(m_Index);
        m_DepthStencilOutput = &resource;
        return resource;
    }

    RDGTextureResource& RenderPass::SetDepthStencilInput(const std::string& name)
    {
        RDGTextureResource& resource = m_Graph.GetOrCreateTextureResource(name);
        resource.AddUsage(RHITextureCreateFlags::ShaderResource);
        resource.MarkReadInPass(m_Index);
        m_DepthStencilInput = &resource;
        return resource;
    }

    RDGTextureResource& RenderPass::AddColorOutput(
        const std::string& name,
        const RDGAttachmentInfo& info,
        const std::string& colorInput)
    {
        RDGTextureResource& resource = m_Graph.GetOrCreateTextureResource(name);
        resource.SetAttachmentInfo(info);
        // Color outputs are sampled by later passes / Editor ImGui viewport.
        resource.AddUsage(RHITextureCreateFlags::RenderTarget | RHITextureCreateFlags::ShaderResource);
        resource.MarkWrittenInPass(m_Index);
        if (!colorInput.empty())
        {
            resource.SetColorInputAlias(colorInput);
            RDGTextureResource& inputResource = m_Graph.GetOrCreateTextureResource(colorInput);
            inputResource.MarkReadInPass(m_Index);
        }
        m_ColorOutputs.push_back(&resource);
        return resource;
    }

    RDGTextureResource& RenderPass::AddTextureInput(const std::string& name)
    {
        RDGTextureResource& resource = m_Graph.GetOrCreateTextureResource(name);
        resource.AddUsage(RHITextureCreateFlags::ShaderResource);
        resource.MarkReadInPass(m_Index);
        m_TextureInputs.push_back(&resource);
        return resource;
    }

    void RenderPass::SetImplementation(IRenderPass* implementation)
    {
        m_OwnedImplementation.reset();
        m_ImplementationPtr = implementation;
    }

    void RenderPass::SetImplementation(std::unique_ptr<IRenderPass> implementation)
    {
        m_OwnedImplementation = std::move(implementation);
        m_ImplementationPtr = m_OwnedImplementation.get();
    }

    IRenderPass* RenderPass::GetImplementation() const
    {
        return m_ImplementationPtr;
    }

    void RenderPass::ClearDeclaredDependencies()
    {
        m_ColorOutputs.clear();
        m_TextureInputs.clear();
        m_DepthStencilOutput = nullptr;
        m_DepthStencilInput = nullptr;
    }

    void RenderPass::RunSetupDependencies()
    {
        ClearDeclaredDependencies();
        if (m_ImplementationPtr == nullptr)
        {
            return;
        }
        m_ImplementationPtr->SetupDependencies(*this, m_Graph);
    }

    void RenderPass::RunSetup(RHI& rhi)
    {
        if (m_ImplementationPtr == nullptr)
        {
            return;
        }
        m_ImplementationPtr->Setup(rhi);
    }

    void RenderPass::RunPrepare()
    {
        if (m_ImplementationPtr == nullptr)
        {
            return;
        }
        m_ImplementationPtr->Prepare(m_Graph);
    }

    void RenderPass::RunBuildRenderPass(RHICommandList& cmdList)
    {
        if (m_ImplementationPtr == nullptr)
        {
            return;
        }
        if (!m_ImplementationPtr->NeedRenderPass())
        {
            return;
        }
        m_ImplementationPtr->BuildRenderPass(cmdList, m_Graph);
    }
}
