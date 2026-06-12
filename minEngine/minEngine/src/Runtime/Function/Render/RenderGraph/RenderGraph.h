#pragma once

#include "Core.h"
#include "Render/RenderGraph/RDGTexture.h"
#include "Render/RenderGraph/RenderPass.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    class RHITexture;
    class RHICommandList;
    class RenderGraphFrameResources;
    class IRenderPass;

    struct RDGTextureRegistryEntry
    {
        std::string Name;
        RDGTextureDesc Desc{};
        bool IsExternal = false;
        RHITexture* ExternalTexture = nullptr;
    };

    /** Manual frame graph orchestrator (RND-F01). */
    class RenderGraph
    {
    public:
        RenderGraph() = default;

        void Reset();

        RenderPass& AddPass(const char* name);
        RenderPass& AddPass(const char* name, std::unique_ptr<IRenderPass> implementation);

        void RegisterExternalTexture(const char* name, RHITexture* texture);

        void SetPassExecutionOrder(const RenderPass* const* passes, size_t passCount);

        void SetupAttachments(RenderGraphFrameResources& frameResources);
        void ExecuteGraph(RHICommandList& cmdList, RenderGraphFrameResources& frameResources);

        RDGTextureRef FindOrRegisterTexture(const char* name, const RDGTextureDesc& desc, bool isExternal);
        const RDGTextureRegistryEntry* FindTextureEntry(const char* name) const;

        const std::vector<std::unique_ptr<RenderPass>>& GetPasses() const { return m_Passes; }

    private:
        RenderPass* FindPass(const char* name) const;
        void RunPassSetup(RenderPass& pass);

        std::vector<std::unique_ptr<RenderPass>> m_Passes;
        std::unordered_map<std::string, RenderPass*> m_PassByName;
        std::vector<const RenderPass*> m_ExecutionOrder;

        std::vector<RDGTextureRegistryEntry> m_TextureRegistry;
        std::unordered_map<std::string, uint32_t> m_TextureIndexByName;
    };
}
