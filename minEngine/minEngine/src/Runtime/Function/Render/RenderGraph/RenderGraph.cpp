#include "Render/RenderGraph/RenderGraph.h"

#include "Render/RHI/RHI.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIResourceTransition.h"
#include "Render/RHI/RHITexture.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace minEngine
{
    void RenderGraph::Reset()
    {
        m_Passes.clear();
        m_PassByName.clear();
        m_ForcedPassNames.clear();
        m_Resources.clear();
        m_NameToResource.clear();
        m_PassStack.clear();
        m_PassDependencies.clear();
        m_PhysicalDims.clear();
        m_PhysicalTextures.clear();
        m_SwapchainPhysicalIndex = RDGResource::kUnused;
        m_IsBaked = false;
        m_ImplSetupDone = false;
        m_FrameContext = {};
    }

    void RenderGraph::SetFrameContext(const RenderGraphFrameContext& context)
    {
        m_FrameContext = context;
    }

    void RenderGraph::SetBackbufferSource(const std::string& logicalTextureName)
    {
        m_BackbufferSource = logicalTextureName;
        m_IsBaked = false;
        m_ImplSetupDone = false;
    }

    void RenderGraph::SetBackbufferDimensions(uint32_t width, uint32_t height)
    {
        m_BackbufferWidth = width;
        m_BackbufferHeight = height;
        m_IsBaked = false;
        m_ImplSetupDone = false;
    }

    RenderPass& RenderGraph::AddPass(const std::string& name, RDGQueue queue)
    {
        if (m_PassByName.find(name) != m_PassByName.end())
        {
            throw std::logic_error("RenderGraph::AddPass: duplicate pass name '" + name + "'.");
        }

        const uint32_t index = static_cast<uint32_t>(m_Passes.size());
        auto pass = std::make_unique<RenderPass>(*this, index, name, queue);
        RenderPass* raw = pass.get();
        m_Passes.push_back(std::move(pass));
        m_PassByName.emplace(name, raw);
        m_IsBaked = false;
        m_ImplSetupDone = false;
        return *raw;
    }

    RenderPass* RenderGraph::FindPass(const std::string& name)
    {
        const auto it = m_PassByName.find(name);
        return it != m_PassByName.end() ? it->second : nullptr;
    }

    void RenderGraph::ForceIncludePass(const std::string& name)
    {
        m_ForcedPassNames.insert(name);
        m_IsBaked = false;
        m_ImplSetupDone = false;
    }

    RDGTextureResource& RenderGraph::GetOrCreateTextureResource(const std::string& name)
    {
        if (RDGTextureResource* existing = FindTextureResource(name))
        {
            return *existing;
        }

        const uint32_t logicalIndex = static_cast<uint32_t>(m_Resources.size());
        auto resource = std::make_unique<RDGTextureResource>(logicalIndex, name);
        RDGTextureResource* raw = resource.get();
        m_Resources.push_back(std::move(resource));
        m_NameToResource.emplace(name, logicalIndex);
        return *raw;
    }

    RDGTextureResource* RenderGraph::FindTextureResource(const std::string& name)
    {
        const auto it = m_NameToResource.find(name);
        if (it == m_NameToResource.end())
        {
            return nullptr;
        }

        RDGResource* resource = m_Resources[it->second].get();
        return static_cast<RDGTextureResource*>(resource);
    }

    RDGTextureResource& RenderGraph::GetTextureResource(const std::string& name)
    {
        RDGTextureResource* resource = FindTextureResource(name);
        if (resource == nullptr)
        {
            throw std::logic_error("RenderGraph::GetTextureResource: unknown '" + name + "'.");
        }
        return *resource;
    }

    void RenderGraph::ValidatePasses() const
    {
        if (m_BackbufferSource.empty())
        {
            throw std::logic_error("RenderGraph::Bake: backbuffer source is not set.");
        }

        const auto it = m_NameToResource.find(m_BackbufferSource);
        if (it == m_NameToResource.end())
        {
            throw std::logic_error(
                "RenderGraph::Bake: backbuffer source '" + m_BackbufferSource + "' does not exist.");
        }

        const RDGResource& backbuffer = *m_Resources[it->second];
        if (backbuffer.GetWritePasses().empty())
        {
            throw std::logic_error("RenderGraph::Bake: no pass writes to backbuffer source.");
        }
    }

    void RenderGraph::RecordPassDependency(uint32_t dependentPass, uint32_t producerPass)
    {
        if (dependentPass == producerPass || dependentPass >= m_PassDependencies.size()
            || producerPass >= m_PassDependencies.size())
        {
            return;
        }
        m_PassDependencies[dependentPass].insert(producerPass);
    }

    void RenderGraph::TraverseDependencies(uint32_t passIndex, std::vector<bool>& visited)
    {
        if (visited[passIndex])
        {
            return;
        }
        visited[passIndex] = true;

        const RenderPass& pass = *m_Passes[passIndex];
        auto considerResource = [this, passIndex, &visited](
                                    const RDGTextureResource* resource,
                                    bool requireWriter) {
            if (resource == nullptr)
            {
                return;
            }
            const std::unordered_set<uint32_t>& writers = resource->GetWritePasses();
            if (requireWriter && writers.empty())
            {
                throw std::logic_error(
                    "RenderGraph::Bake: no pass writes to resource '" + resource->GetName() + "'.");
            }
            for (uint32_t writer : writers)
            {
                RecordPassDependency(passIndex, writer);
                TraverseDependencies(writer, visited);
            }
        };

        for (RDGTextureResource* input : pass.GetTextureInputs())
        {
            considerResource(input, true);
        }
        considerResource(pass.GetDepthStencilInput(), true);
        for (RDGTextureResource* output : pass.GetColorOutputs())
        {
            if (output != nullptr && !output->GetColorInputAlias().empty())
            {
                considerResource(FindTextureResource(output->GetColorInputAlias()), true);
            }
        }

        m_PassStack.push_back(passIndex);
    }

    void RenderGraph::FilterPassStack()
    {
        std::vector<uint32_t> unique;
        unique.reserve(m_PassStack.size());
        std::vector<bool> seen(m_Passes.size(), false);
        for (uint32_t passIndex : m_PassStack)
        {
            if (seen[passIndex])
            {
                continue;
            }
            seen[passIndex] = true;
            unique.push_back(passIndex);
        }
        m_PassStack = std::move(unique);
    }

    RDGResourceDimensions RenderGraph::ResolveDimensions(const RDGTextureResource& resource) const
    {
        if (!resource.HasAttachmentInfo())
        {
            throw std::logic_error(
                "RenderGraph::Bake: texture '" + resource.GetName() + "' has no attachment info.");
        }

        const RDGAttachmentInfo& info = resource.GetAttachmentInfo();
        RDGResourceDimensions dims{};
        dims.Format = info.Format;
        dims.Layers = info.Layers;
        dims.Levels = info.Levels;
        dims.Samples = info.Samples;
        dims.Flags = info.Flags;
        dims.Usage = resource.GetUsage();
        dims.Dimension = info.Dimension;
        dims.DebugName = resource.GetName();

        if (info.SizeClass == RDGSizeClass::Absolute)
        {
            dims.Width = static_cast<uint32_t>(std::lround(info.SizeX));
            dims.Height = static_cast<uint32_t>(std::lround(info.SizeY));
        }
        else if (info.SizeClass == RDGSizeClass::SwapchainRelative)
        {
            if (m_BackbufferWidth == 0 || m_BackbufferHeight == 0)
            {
                throw std::logic_error("RenderGraph::Bake: swapchain-relative size needs SetBackbufferDimensions.");
            }
            dims.Width = std::max(1u, static_cast<uint32_t>(std::lround(m_BackbufferWidth * info.SizeX)));
            dims.Height = std::max(1u, static_cast<uint32_t>(std::lround(m_BackbufferHeight * info.SizeY)));
        }
        else
        {
            throw std::logic_error("RenderGraph::Bake: InputRelative size is deferred (RND-F07).");
        }

        if (dims.Format == TextureFormat::None)
        {
            throw std::logic_error(
                "RenderGraph::Bake: texture '" + resource.GetName() + "' has undefined format.");
        }
        if (dims.Width == 0 || dims.Height == 0)
        {
            throw std::logic_error(
                "RenderGraph::Bake: texture '" + resource.GetName() + "' resolved to zero size.");
        }
        return dims;
    }

    void RenderGraph::AllocatePhysicalForTexture(RDGTextureResource* texture)
    {
        if (texture == nullptr || texture->GetPhysicalIndex() != RDGResource::kUnused)
        {
            return;
        }

        if (!texture->GetColorInputAlias().empty())
        {
            RDGTextureResource* aliasSource = FindTextureResource(texture->GetColorInputAlias());
            if (aliasSource == nullptr)
            {
                throw std::logic_error(
                    "RenderGraph::Bake: color input alias '" + texture->GetColorInputAlias()
                    + "' missing for '" + texture->GetName() + "'.");
            }
            AllocatePhysicalForTexture(aliasSource);
            texture->SetPhysicalIndex(aliasSource->GetPhysicalIndex());
            return;
        }

        RDGResourceDimensions dims = ResolveDimensions(*texture);
        const uint32_t physicalIndex = static_cast<uint32_t>(m_PhysicalDims.size());
        m_PhysicalDims.push_back(std::move(dims));
        texture->SetPhysicalIndex(physicalIndex);
    }

    void RenderGraph::BuildPhysicalResources()
    {
        m_PhysicalDims.clear();
        for (std::unique_ptr<RDGResource>& resource : m_Resources)
        {
            resource->SetPhysicalIndex(RDGResource::kUnused);
        }

        for (uint32_t passIndex : m_PassStack)
        {
            RenderPass& pass = *m_Passes[passIndex];
            for (RDGTextureResource* output : pass.GetColorOutputs())
            {
                AllocatePhysicalForTexture(output);
            }
            AllocatePhysicalForTexture(pass.GetDepthStencilOutput());
        }

        const auto backbufferIt = m_NameToResource.find(m_BackbufferSource);
        m_SwapchainPhysicalIndex = m_Resources[backbufferIt->second]->GetPhysicalIndex();
    }

    void RenderGraph::Bake()
    {
        for (std::unique_ptr<RDGResource>& resource : m_Resources)
        {
            resource->ClearPassUsage();
            resource->SetPhysicalIndex(RDGResource::kUnused);
            if (RDGTextureResource* texture = dynamic_cast<RDGTextureResource*>(resource.get()))
            {
                texture->SetColorInputAlias({});
            }
        }

        for (std::unique_ptr<RenderPass>& pass : m_Passes)
        {
            pass->RunSetupDependencies();
        }

        ValidatePasses();

        m_PassStack.clear();
        m_PassDependencies.assign(m_Passes.size(), {});
        std::vector<bool> visited(m_Passes.size(), false);

        for (const std::string& forcedName : m_ForcedPassNames)
        {
            RenderPass* forcedPass = FindPass(forcedName);
            if (forcedPass == nullptr)
            {
                throw std::logic_error("RenderGraph::Bake: ForceIncludePass unknown '" + forcedName + "'.");
            }
            TraverseDependencies(forcedPass->GetIndex(), visited);
        }

        const RDGResource& backbuffer = *m_Resources[m_NameToResource[m_BackbufferSource]];
        std::vector<uint32_t> writers(
            backbuffer.GetWritePasses().begin(),
            backbuffer.GetWritePasses().end());
        std::sort(writers.begin(), writers.end());
        for (uint32_t writer : writers)
        {
            TraverseDependencies(writer, visited);
        }

        // Traverse pushes dependents after dependencies; stack is dependency-first order.
        FilterPassStack();
        BuildPhysicalResources();

        // Keep existing physical textures across rebakes so ImGui can keep sampling last frame's
        // handle until SetupAttachments replaces mismatched slots.
        if (m_PhysicalTextures.size() > m_PhysicalDims.size())
        {
            m_PhysicalTextures.resize(m_PhysicalDims.size());
        }
        else if (m_PhysicalTextures.size() < m_PhysicalDims.size())
        {
            m_PhysicalTextures.resize(m_PhysicalDims.size(), nullptr);
        }

        m_IsBaked = true;
        m_ImplSetupDone = false;
    }

    void RenderGraph::InvalidateBake()
    {
        m_IsBaked = false;
        m_ImplSetupDone = false;
    }

    RHITextureCreateDesc RenderGraph::MakeCreateDesc(const RDGResourceDimensions& dims) const
    {
        RHITextureCreateDesc desc{};
        desc.Dimension = dims.Dimension;
        desc.Width = dims.Width;
        desc.Height = dims.Height;
        desc.DepthOrArrayLayers = dims.Layers;
        desc.Format = dims.Format;
        desc.Flags = dims.Usage;
        desc.NumMips = dims.Levels;

        // Color RTs and sampleable depth (shadow maps) keep ShaderResource from declared usage.
        const bool isDepth =
            dims.Format == TextureFormat::DEPTH16 || dims.Format == TextureFormat::DEPTH24
            || dims.Format == TextureFormat::DEPTH32 || dims.Format == TextureFormat::DEPTH24STENCIL8;
        if (!isDepth)
        {
            desc.Flags = desc.Flags | RHITextureCreateFlags::ShaderResource;
        }
        if ((desc.Flags & RHITextureCreateFlags::RenderTarget) == RHITextureCreateFlags::None
            && (desc.Flags & RHITextureCreateFlags::ShaderResource) == RHITextureCreateFlags::None)
        {
            desc.Flags = RHITextureCreateFlags::RenderTarget | RHITextureCreateFlags::ShaderResource;
        }
        return desc;
    }

    void RenderGraph::SetupAttachments(RHI& rhi, RHITexture* swapchainOrNull)
    {
        if (!m_IsBaked)
        {
            throw std::logic_error("RenderGraph::SetupAttachments: Bake() required first.");
        }

        if (!m_ImplSetupDone)
        {
            for (uint32_t passIndex : m_PassStack)
            {
                m_Passes[passIndex]->RunSetup(rhi);
            }
            m_ImplSetupDone = true;
        }

        if (m_PhysicalTextures.size() != m_PhysicalDims.size())
        {
            m_PhysicalTextures.assign(m_PhysicalDims.size(), nullptr);
        }

        for (uint32_t physicalIndex = 0; physicalIndex < m_PhysicalDims.size(); ++physicalIndex)
        {
            if (swapchainOrNull != nullptr && physicalIndex == m_SwapchainPhysicalIndex)
            {
                // Non-owning view of external swapchain/backbuffer when provided.
                // Store as shared_ptr aliasing no-op deleter? Prefer keep separate path:
                // For S04 tests we allocate all physical textures including backbuffer source.
                (void)swapchainOrNull;
            }

            const RDGResourceDimensions& dims = m_PhysicalDims[physicalIndex];
            std::shared_ptr<RHITexture>& slot = m_PhysicalTextures[physicalIndex];
            const RHITextureCreateDesc wantDesc = MakeCreateDesc(dims);
            if (slot
                && slot->GetDesc().Width == dims.Width
                && slot->GetDesc().Height == dims.Height
                && slot->GetDesc().Format == dims.Format
                && slot->GetDesc().DepthOrArrayLayers == dims.Layers
                && (slot->GetDesc().Flags & wantDesc.Flags) == wantDesc.Flags)
            {
                continue;
            }

            slot = rhi.RHICreateTexture2D(wantDesc, nullptr);
            if (!slot)
            {
                throw std::runtime_error(
                    "RenderGraph::SetupAttachments: failed to create '" + dims.DebugName + "'.");
            }
        }
    }

    void RenderGraph::InsertPassInputBarriers(RHICommandList& cmdList, const RenderPass& pass)
    {
        auto transitionTexture = [&](RDGTextureResource* resource)
        {
            RHITexture* physical = TryGetPhysicalTexture(resource);
            if (physical == nullptr)
            {
                return;
            }

            RHITextureTransitionInfo info{};
            info.Texture = physical;
            cmdList.Transition(info);
        };

        for (RDGTextureResource* input : pass.GetTextureInputs())
        {
            transitionTexture(input);
        }
        transitionTexture(pass.GetDepthStencilInput());
        for (RDGTextureResource* output : pass.GetColorOutputs())
        {
            if (output != nullptr && !output->GetColorInputAlias().empty())
            {
                transitionTexture(FindTextureResource(output->GetColorInputAlias()));
            }
        }
    }

    void RenderGraph::EnqueueRenderPasses(RHICommandList& cmdList)
    {
        if (!m_IsBaked)
        {
            throw std::logic_error("RenderGraph::EnqueueRenderPasses: Bake() required first.");
        }

        for (uint32_t passIndex : m_PassStack)
        {
            m_Passes[passIndex]->RunPrepare();
        }
        for (uint32_t passIndex : m_PassStack)
        {
            InsertPassInputBarriers(cmdList, *m_Passes[passIndex]);
            m_Passes[passIndex]->RunBuildRenderPass(cmdList);
        }
    }

    RHITexture* RenderGraph::GetPhysicalTexture(const RDGTextureResource& resource)
    {
        return GetPhysicalTexture(resource.GetPhysicalIndex());
    }

    RHITexture* RenderGraph::GetPhysicalTexture(uint32_t physicalIndex)
    {
        if (physicalIndex == RDGResource::kUnused || physicalIndex >= m_PhysicalTextures.size())
        {
            throw std::logic_error("RenderGraph::GetPhysicalTexture: invalid physical index.");
        }
        RHITexture* texture = m_PhysicalTextures[physicalIndex].get();
        if (texture == nullptr)
        {
            throw std::logic_error("RenderGraph::GetPhysicalTexture: SetupAttachments not called.");
        }
        return texture;
    }

    RHITexture* RenderGraph::TryGetPhysicalTexture(RDGTextureResource* resource)
    {
        if (resource == nullptr || resource->GetPhysicalIndex() == RDGResource::kUnused)
        {
            return nullptr;
        }
        if (resource->GetPhysicalIndex() >= m_PhysicalTextures.size())
        {
            return nullptr;
        }
        return m_PhysicalTextures[resource->GetPhysicalIndex()].get();
    }

    RHITextureRef RenderGraph::GetPhysicalTextureShared(const RDGTextureResource& resource)
    {
        return GetPhysicalTextureShared(resource.GetPhysicalIndex());
    }

    RHITextureRef RenderGraph::GetPhysicalTextureShared(uint32_t physicalIndex)
    {
        if (physicalIndex == RDGResource::kUnused || physicalIndex >= m_PhysicalTextures.size())
        {
            throw std::logic_error("RenderGraph::GetPhysicalTextureShared: invalid physical index.");
        }
        RHITextureRef texture = m_PhysicalTextures[physicalIndex];
        if (!texture)
        {
            throw std::logic_error("RenderGraph::GetPhysicalTextureShared: SetupAttachments not called.");
        }
        return texture;
    }

    const RDGResourceDimensions& RenderGraph::GetPhysicalDimensions(uint32_t physicalIndex) const
    {
        if (physicalIndex >= m_PhysicalDims.size())
        {
            throw std::logic_error("RenderGraph::GetPhysicalDimensions: invalid physical index.");
        }
        return m_PhysicalDims[physicalIndex];
    }
}
