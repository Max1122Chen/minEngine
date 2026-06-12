#pragma once

#include "Core.h"
#include "Render/RenderGraph/PassParameters.h"
#include "Render/RenderGraph/RDGTexture.h"
#include "Render/RenderGraph/RenderGraphFrameContext.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace minEngine
{
    class RenderPass;

    class RHICommandList;

    class RenderGraphFrameResources
    {
    public:
        void Clear();

        void BeginFrame(RHICommandList& cmdList) { m_ActiveCommandList = &cmdList; }

        RHICommandList& GetCommandList() const { return *m_ActiveCommandList; }

        void SetFrameContext(const FrameRenderGraphContext& context) { m_FrameContext = context; }

        const FrameRenderGraphContext& GetFrameContext() const { return m_FrameContext; }

        void RegisterExternal(const char* name, RHITexture* texture);
        void EnsureSlot(const char* name);

        RHITexture* GetRHI(const char* name) const;
        RDGTextureUsage GetLastKnownUsage(const char* name) const;
        void SetLastKnownUsage(const char* name, RDGTextureUsage usage);
        void SetSRV(const char* name, RHIShaderResourceViewRef srv);

        PassParameters& GetOrCreatePassParameters(const RenderPass& pass) const;

        template <typename T>
        T& GetPassParameters(RenderPass& pass)
        {
            return static_cast<T&>(GetOrCreatePassParameters(pass));
        }

    private:
        RDGTextureSlot* FindMutableSlot(const char* name);
        const RDGTextureSlot* FindSlot(const char* name) const;

        std::unordered_map<std::string, RDGTextureSlot> m_TextureSlots;
        mutable std::unordered_map<const RenderPass*, std::unique_ptr<PassParameters>> m_PassParameters;

        RHICommandList* m_ActiveCommandList = nullptr;
        FrameRenderGraphContext m_FrameContext{};
    };
}
