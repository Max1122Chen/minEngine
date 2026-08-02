#pragma once

#include "Core.h"
#include "Render/RenderGraph/RDGResource.h"
#include "Render/RenderGraph/RDGTypes.h"
#include "Render/RenderGraph/RenderGraphFrameContext.h"
#include "Render/RenderGraph/RenderPass.h"
#include "Render/RHI/RHITexture.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace minEngine
{
    class RHI;
    class RHICommandList;

    /**
     * Granite-style frame render graph (RND-F07).
     * Owns logical→physical texture mapping after Bake/SetupAttachments.
     */
    class RenderGraph
    {
    public:
        RenderGraph() = default;

        void Reset();

        void SetBackbufferSource(const std::string& logicalTextureName);
        void SetBackbufferDimensions(uint32_t width, uint32_t height);
        void SetFrameContext(const RenderGraphFrameContext& context);
        const RenderGraphFrameContext& GetFrameContext() const { return m_FrameContext; }

        RenderPass& AddPass(const std::string& name, RDGQueue queue = RDGQueue::Graphics);
        RenderPass* FindPass(const std::string& name);
        void ForceIncludePass(const std::string& name);

        RDGTextureResource& GetOrCreateTextureResource(const std::string& name);
        RDGTextureResource* FindTextureResource(const std::string& name);
        RDGTextureResource& GetTextureResource(const std::string& name);

        void Bake();
        void SetupAttachments(RHI& rhi, RHITexture* swapchainOrNull);
        void EnqueueRenderPasses(RHICommandList& cmdList);

        RHITexture* GetPhysicalTexture(const RDGTextureResource& resource);
        RHITexture* GetPhysicalTexture(uint32_t physicalIndex);
        RHITexture* TryGetPhysicalTexture(RDGTextureResource* resource);
        RHITextureRef GetPhysicalTextureShared(const RDGTextureResource& resource);
        RHITextureRef GetPhysicalTextureShared(uint32_t physicalIndex);

        const RDGResourceDimensions& GetPhysicalDimensions(uint32_t physicalIndex) const;
        bool IsBaked() const { return m_IsBaked; }

        uint32_t GetBackbufferWidth() const { return m_BackbufferWidth; }
        uint32_t GetBackbufferHeight() const { return m_BackbufferHeight; }

    private:
        friend class RenderPass;

        void ValidatePasses() const;
        void TraverseDependencies(uint32_t passIndex, std::vector<bool>& visited);
        void FilterPassStack();
        void BuildPhysicalResources();
        void AllocatePhysicalForTexture(RDGTextureResource* texture);
        RDGResourceDimensions ResolveDimensions(const RDGTextureResource& resource) const;
        RHITextureCreateDesc MakeCreateDesc(const RDGResourceDimensions& dims) const;

        std::vector<std::unique_ptr<RenderPass>> m_Passes;
        std::unordered_map<std::string, RenderPass*> m_PassByName;
        std::unordered_set<std::string> m_ForcedPassNames;

        std::vector<std::unique_ptr<RDGResource>> m_Resources;
        std::unordered_map<std::string, uint32_t> m_NameToResource;

        std::string m_BackbufferSource;
        uint32_t m_BackbufferWidth = 0;
        uint32_t m_BackbufferHeight = 0;
        RenderGraphFrameContext m_FrameContext{};

        std::vector<uint32_t> m_PassStack;
        std::vector<RDGResourceDimensions> m_PhysicalDims;
        std::vector<std::shared_ptr<RHITexture>> m_PhysicalTextures;
        uint32_t m_SwapchainPhysicalIndex = RDGResource::kUnused;
        bool m_IsBaked = false;
        bool m_ImplSetupDone = false;
    };
}
