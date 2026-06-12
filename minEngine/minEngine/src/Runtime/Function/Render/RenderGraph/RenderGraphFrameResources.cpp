#include "Render/RenderGraph/RenderGraphFrameResources.h"

#include "Render/RenderGraph/RenderPass.h"

namespace minEngine
{
    void RenderGraphFrameResources::Clear()
    {
        m_TextureSlots.clear();
        m_PassParameters.clear();
    }

    void RenderGraphFrameResources::RegisterExternal(const char* name, RHITexture* texture)
    {
        if (name == nullptr || name[0] == '\0')
        {
            return;
        }

        RDGTextureSlot& slot = m_TextureSlots[name];
        slot.Name = name;
        slot.Texture = texture;
    }

    void RenderGraphFrameResources::EnsureSlot(const char* name)
    {
        if (name == nullptr || name[0] == '\0')
        {
            return;
        }

        RDGTextureSlot& slot = m_TextureSlots[name];
        if (slot.Name.empty())
        {
            slot.Name = name;
        }
    }

    RHITexture* RenderGraphFrameResources::GetRHI(const char* name) const
    {
        const RDGTextureSlot* slot = FindSlot(name);
        return slot != nullptr ? slot->Texture : nullptr;
    }

    RDGTextureUsage RenderGraphFrameResources::GetLastKnownUsage(const char* name) const
    {
        const RDGTextureSlot* slot = FindSlot(name);
        return slot != nullptr ? slot->LastKnownUsage : RDGTextureUsage::Unknown;
    }

    void RenderGraphFrameResources::SetLastKnownUsage(const char* name, RDGTextureUsage usage)
    {
        RDGTextureSlot* slot = FindMutableSlot(name);
        if (slot != nullptr)
        {
            slot->LastKnownUsage = usage;
        }
    }

    void RenderGraphFrameResources::SetSRV(const char* name, RHIShaderResourceViewRef srv)
    {
        RDGTextureSlot* slot = FindMutableSlot(name);
        if (slot != nullptr)
        {
            slot->SRV = std::move(srv);
        }
    }

    PassParameters& RenderGraphFrameResources::GetOrCreatePassParameters(const RenderPass& pass) const
    {
        std::unique_ptr<PassParameters>& parameters = m_PassParameters[&pass];
        if (!parameters)
        {
            parameters = std::make_unique<PassParameters>();
        }
        return *parameters;
    }

    RDGTextureSlot* RenderGraphFrameResources::FindMutableSlot(const char* name)
    {
        if (name == nullptr)
        {
            return nullptr;
        }

        auto it = m_TextureSlots.find(name);
        return it != m_TextureSlots.end() ? &it->second : nullptr;
    }

    const RDGTextureSlot* RenderGraphFrameResources::FindSlot(const char* name) const
    {
        if (name == nullptr)
        {
            return nullptr;
        }

        auto it = m_TextureSlots.find(name);
        return it != m_TextureSlots.end() ? &it->second : nullptr;
    }
}
