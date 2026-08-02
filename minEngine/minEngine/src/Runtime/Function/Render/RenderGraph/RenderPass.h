#pragma once

#include "Core.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RDGResource.h"
#include "Render/RenderGraph/RDGTypes.h"

#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class RenderGraph;
    class RHI;
    class RHICommandList;

    /** RenderGraph step node (Granite RenderPass). Not Legacy RenderPasses/* classes. */
    class RenderPass
    {
    public:
        RenderPass(RenderGraph& graph, uint32_t index, std::string name, RDGQueue queue);

        const std::string& GetName() const { return m_Name; }
        uint32_t GetIndex() const { return m_Index; }
        RDGQueue GetQueue() const { return m_Queue; }
        RenderGraph& GetGraph() const { return m_Graph; }

        RDGTextureResource& SetDepthStencilOutput(const std::string& name, const RDGAttachmentInfo& info);
        RDGTextureResource& SetDepthStencilInput(const std::string& name);
        RDGTextureResource& AddColorOutput(
            const std::string& name,
            const RDGAttachmentInfo& info,
            const std::string& colorInput = {});
        RDGTextureResource& AddTextureInput(const std::string& name);

        const std::vector<RDGTextureResource*>& GetColorOutputs() const { return m_ColorOutputs; }
        const std::vector<RDGTextureResource*>& GetTextureInputs() const { return m_TextureInputs; }
        RDGTextureResource* GetDepthStencilOutput() const { return m_DepthStencilOutput; }
        RDGTextureResource* GetDepthStencilInput() const { return m_DepthStencilInput; }

        void SetImplementation(IRenderPass* implementation);
        void SetImplementation(std::unique_ptr<IRenderPass> implementation);
        IRenderPass* GetImplementation() const;

        void RunSetupDependencies();
        void RunSetup(RHI& rhi);
        void RunPrepare();
        void RunBuildRenderPass(RHICommandList& cmdList);
        void ClearDeclaredDependencies();

    private:
        RenderGraph& m_Graph;
        uint32_t m_Index = RDGResource::kUnused;
        std::string m_Name;
        RDGQueue m_Queue = RDGQueue::Graphics;

        IRenderPass* m_ImplementationPtr = nullptr;
        std::unique_ptr<IRenderPass> m_OwnedImplementation;

        std::vector<RDGTextureResource*> m_ColorOutputs;
        std::vector<RDGTextureResource*> m_TextureInputs;
        RDGTextureResource* m_DepthStencilOutput = nullptr;
        RDGTextureResource* m_DepthStencilInput = nullptr;
    };
}
