#include "Render/RenderGraph/RenderGraph.h"

#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RenderGraphFrameResources.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHITexture.h"

#include <stdexcept>
#include <unordered_set>

namespace minEngine
{
    void RenderGraph::Reset()
    {
        m_Passes.clear();
        m_PassByName.clear();
        m_SelectedPasses.clear();
        m_ExecutionOrder.clear();
        m_IsBaked = false;
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

        const std::string textureName(name);
        auto existing = m_TextureIndexByName.find(textureName);
        if (existing != m_TextureIndexByName.end())
        {
            RDGTextureRegistryEntry& entry = m_TextureRegistry[existing->second];
            entry.IsExternal = true;
            entry.ExternalTexture = texture;
            if (texture != nullptr)
            {
                const RHITextureCreateDesc& createDesc = texture->GetDesc();
                entry.Desc.Width = createDesc.Width;
                entry.Desc.Height = createDesc.Height;
            }
            return;
        }

        RDGTextureRef textureRef = FindOrRegisterTexture(name, desc, true);
        RDGTextureRegistryEntry& entry = m_TextureRegistry[textureRef.GetIndex()];
        entry.IsExternal = true;
        entry.ExternalTexture = texture;
    }

    void RenderGraph::SetPassExecutionOrder(const RenderPass* const* passes, size_t passCount)
    {
        m_SelectedPasses.clear();
        m_ExecutionOrder.clear();
        m_IsBaked = false;
        if (passes == nullptr)
        {
            return;
        }

        m_SelectedPasses.reserve(passCount);
        for (size_t i = 0; i < passCount; ++i)
        {
            if (passes[i] != nullptr)
            {
                m_SelectedPasses.push_back(passes[i]);
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

    std::vector<const RenderPass*> RenderGraph::CollectActivePasses() const
    {
        std::vector<const RenderPass*> activePasses;
        activePasses.reserve(m_Passes.size());

        if (m_SelectedPasses.empty())
        {
            for (const std::unique_ptr<RenderPass>& pass : m_Passes)
            {
                activePasses.push_back(pass.get());
            }
            return activePasses;
        }

        std::unordered_set<const RenderPass*> selectedSet;
        selectedSet.reserve(m_SelectedPasses.size());
        for (const RenderPass* pass : m_SelectedPasses)
        {
            if (pass == nullptr)
            {
                continue;
            }

            if (!selectedSet.insert(pass).second)
            {
                continue;
            }

            if (m_PassByName.find(pass->GetName()) == m_PassByName.end())
            {
                throw std::logic_error("RenderGraph::Bake: selected pass does not belong to this graph.");
            }
        }

        for (const std::unique_ptr<RenderPass>& pass : m_Passes)
        {
            if (selectedSet.find(pass.get()) != selectedSet.end())
            {
                activePasses.push_back(pass.get());
            }
        }

        return activePasses;
    }

    void RenderGraph::Bake()
    {
        for (const std::unique_ptr<RenderPass>& pass : m_Passes)
        {
            RunPassSetup(*pass);
        }

        std::vector<const RenderPass*> activePasses = CollectActivePasses();
        m_ExecutionOrder.clear();
        if (activePasses.empty())
        {
            m_IsBaked = true;
            return;
        }

        struct ResourceState
        {
            bool HasProducer = false;
            const RenderPass* LastWriter = nullptr;
            const RenderPass* LastAccessor = nullptr;
        };

        std::unordered_map<const RenderPass*, size_t> passIndex;
        passIndex.reserve(activePasses.size());
        for (size_t index = 0; index < activePasses.size(); ++index)
        {
            passIndex.emplace(activePasses[index], index);
        }

        std::vector<std::unordered_set<size_t>> adjacency(activePasses.size());
        std::vector<size_t> indegree(activePasses.size(), 0);
        std::unordered_map<std::string, ResourceState> resourceStates;
        resourceStates.reserve(m_TextureRegistry.size());
        for (const RDGTextureRegistryEntry& entry : m_TextureRegistry)
        {
            ResourceState& state = resourceStates[entry.Name];
            state.HasProducer = entry.IsExternal;
        }

        for (const RenderPass* pass : activePasses)
        {
            const size_t currentIndex = passIndex.at(pass);
            for (const PassResourceAccess& access : pass->GetDeclaredAccesses())
            {
                ResourceState& state = resourceStates[access.TextureName];
                const bool isWrite = access.AccessType == RDGPassResourceAccessType::ColorOutput
                    || access.AccessType == RDGPassResourceAccessType::DepthStencilOutput;
                const bool isRead = access.AccessType == RDGPassResourceAccessType::TextureInput
                    || access.AccessType == RDGPassResourceAccessType::DepthStencilInput;

                if (isRead && !state.HasProducer)
                {
                    throw std::logic_error(
                        "RenderGraph::Bake: texture input '" + access.TextureName + "' has no producer or external registration.");
                }

                const RenderPass* predecessor = nullptr;
                if (isRead)
                {
                    predecessor = state.LastWriter;
                }
                else if (isWrite)
                {
                    predecessor = state.LastAccessor;
                }

                if (predecessor != nullptr)
                {
                    const size_t predecessorIndex = passIndex.at(predecessor);
                    if (predecessorIndex != currentIndex && adjacency[predecessorIndex].insert(currentIndex).second)
                    {
                        ++indegree[currentIndex];
                    }
                }

                if (isWrite)
                {
                    state.HasProducer = true;
                    state.LastWriter = pass;
                }
                state.LastAccessor = pass;
            }
        }

        std::vector<bool> emitted(activePasses.size(), false);
        m_ExecutionOrder.reserve(activePasses.size());
        for (size_t emittedCount = 0; emittedCount < activePasses.size(); ++emittedCount)
        {
            size_t nextIndex = activePasses.size();
            for (size_t candidateIndex = 0; candidateIndex < activePasses.size(); ++candidateIndex)
            {
                if (!emitted[candidateIndex] && indegree[candidateIndex] == 0)
                {
                    nextIndex = candidateIndex;
                    break;
                }
            }

            if (nextIndex == activePasses.size())
            {
                throw std::logic_error("RenderGraph::Bake: cyclic or unsatisfied dependency graph.");
            }

            emitted[nextIndex] = true;
            m_ExecutionOrder.push_back(activePasses[nextIndex]);
            for (size_t dependencyIndex : adjacency[nextIndex])
            {
                --indegree[dependencyIndex];
            }
        }

        m_IsBaked = true;
    }

    void RenderGraph::ExecuteGraph(RHICommandList& cmdList, RenderGraphFrameResources& frameResources)
    {
        if (!m_IsBaked)
        {
            Bake();
        }

        frameResources.BeginFrame(cmdList);

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
