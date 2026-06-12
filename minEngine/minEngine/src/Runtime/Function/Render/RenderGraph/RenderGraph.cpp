#include "Render/RenderGraph/RenderGraph.h"

#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RenderGraphFrameResources.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHITexture.h"

namespace minEngine
{
    void RenderGraph::Reset()
    {
        m_Passes.clear();
        m_PassByName.clear();
        m_ExecutionOrder.clear();
        m_TextureRegistry.clear();
        m_TextureIndexByName.clear();
    }

    RenderPass& RenderGraph::AddPass(const char* name)
    {
        auto pass = std::make_unique<RenderPass>(name != nullptr ? name : "");
        RenderPass* passPtr = pass.get();
        m_PassByName[passPtr->GetName()] = passPtr;
        m_Passes.push_back(std::move(pass));
        return *passPtr;
    }

    RenderPass& RenderGraph::AddPass(const char* name, std::unique_ptr<IRenderPass> implementation)
    {
        RenderPass& pass = AddPass(name);
        pass.SetImplementation(std::move(implementation));
        return pass;
    }

    void RenderGraph::RegisterExternalTexture(const char* name, RHITexture* texture)
    {
        if (name == nullptr || name[0] == '\0')
        {
            return;
        }

        RDGTextureDesc desc{};
        if (texture != nullptr)
        {
            const RHITextureCreateDesc& createDesc = texture->GetDesc();
            desc.Width = createDesc.Width;
            desc.Height = createDesc.Height;
        }

        RDGTextureRef textureRef = FindOrRegisterTexture(name, desc, true);
        RDGTextureRegistryEntry& entry = m_TextureRegistry[textureRef.GetIndex()];
        entry.IsExternal = true;
        entry.ExternalTexture = texture;
    }

    void RenderGraph::SetPassExecutionOrder(const RenderPass* const* passes, size_t passCount)
    {
        m_ExecutionOrder.clear();
        if (passes == nullptr)
        {
            return;
        }

        m_ExecutionOrder.reserve(passCount);
        for (size_t i = 0; i < passCount; ++i)
        {
            if (passes[i] != nullptr)
            {
                m_ExecutionOrder.push_back(passes[i]);
            }
        }
    }

    void RenderGraph::SetupAttachments(RenderGraphFrameResources& frameResources)
    {
        for (const std::unique_ptr<RenderPass>& pass : m_Passes)
        {
            RunPassSetup(*pass);
        }

        for (const RDGTextureRegistryEntry& entry : m_TextureRegistry)
        {
            frameResources.EnsureSlot(entry.Name.c_str());
            if (entry.IsExternal && entry.ExternalTexture != nullptr)
            {
                frameResources.RegisterExternal(entry.Name.c_str(), entry.ExternalTexture);
            }
        }
    }

    RDGTextureRef RenderGraph::FindOrRegisterTexture(const char* name, const RDGTextureDesc& desc, bool isExternal)
    {
        if (name == nullptr || name[0] == '\0')
        {
            return RDGTextureRef{};
        }

        const std::string textureName(name);
        auto existing = m_TextureIndexByName.find(textureName);
        if (existing != m_TextureIndexByName.end())
        {
            return RDGTextureRef::FromIndex(existing->second);
        }

        RDGTextureRegistryEntry entry;
        entry.Name = textureName;
        entry.Desc = desc;
        entry.IsExternal = isExternal;

        const uint32_t index = static_cast<uint32_t>(m_TextureRegistry.size());
        m_TextureRegistry.push_back(std::move(entry));
        m_TextureIndexByName.emplace(textureName, index);
        return RDGTextureRef::FromIndex(index);
    }

    const RDGTextureRegistryEntry* RenderGraph::FindTextureEntry(const char* name) const
    {
        if (name == nullptr)
        {
            return nullptr;
        }

        auto it = m_TextureIndexByName.find(name);
        if (it == m_TextureIndexByName.end())
        {
            return nullptr;
        }

        return &m_TextureRegistry[it->second];
    }

    RenderPass* RenderGraph::FindPass(const char* name) const
    {
        if (name == nullptr)
        {
            return nullptr;
        }

        auto it = m_PassByName.find(name);
        return it != m_PassByName.end() ? it->second : nullptr;
    }

    void RenderGraph::RunPassSetup(RenderPass& pass)
    {
        pass.RunSetup(*this);
    }

    void RenderGraph::ExecuteGraph(RHICommandList& cmdList, RenderGraphFrameResources& frameResources)
    {
        for (const RenderPass* pass : m_ExecutionOrder)
        {
            if (pass == nullptr)
            {
                continue;
            }

            pass->PreparePass(frameResources);
        }

        for (const RenderPass* pass : m_ExecutionOrder)
        {
            if (pass == nullptr)
            {
                continue;
            }

            const PassParameters& parameters = frameResources.GetOrCreatePassParameters(*pass);
            pass->BuildRenderPass(cmdList, parameters);
        }
    }
}
