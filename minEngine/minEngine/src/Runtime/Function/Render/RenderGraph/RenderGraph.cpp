#include "Render/RenderGraph/RenderGraph.h"

#include "Runtime/Core/Log/LogSystem.h"
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
        m_BoundPhysicalTextures = nullptr;
        m_ActiveViewKey = {};
        // keep per-view physical caches across topology rebuilds; drop external bindings.
        for (ViewPhysicalCache& cache : m_ViewPhysicalCaches)
        {
            cache.Externals.clear();
            cache.ExternalPhysicalIndices.clear();
        }
        m_SwapchainPhysicalIndex = RDGResource::kUnused;
        m_IsBaked = false;
        m_ImplSetupDone = false;
        m_FrameContext = {};
    }

    RenderGraph::ViewPhysicalCache* RenderGraph::TryGetActiveViewCache()
    {
        if (!m_ActiveViewKey.IsValid())
        {
            return nullptr;
        }
        return &FindOrCreateViewCache(m_ActiveViewKey);
    }

    const RenderGraph::ViewPhysicalCache* RenderGraph::TryGetActiveViewCache() const
    {
        if (!m_ActiveViewKey.IsValid())
        {
            return nullptr;
        }
        for (const ViewPhysicalCache& cache : m_ViewPhysicalCaches)
        {
            if (cache.Key == m_ActiveViewKey)
            {
                return &cache;
            }
        }
        return nullptr;
    }

    bool RenderGraph::RegisterExternalTexture(
        const std::string& logicalName,
        const RHITextureRef& texture,
        std::string* outError)
    {
        auto setError = [&](const char* message)
        {
            if (outError != nullptr)
            {
                *outError = message;
            }
        };

        if (logicalName.empty())
        {
            setError("logical name is empty");
            return false;
        }
        if (!texture)
        {
            setError("texture is null");
            return false;
        }
        ViewPhysicalCache* cache = TryGetActiveViewCache();
        if (cache == nullptr)
        {
            setError("no active ViewKey");
            return false;
        }

        if (m_IsBaked)
        {
            RDGTextureResource* resource = FindTextureResource(logicalName);
            if (resource == nullptr || resource->GetPhysicalIndex() == RDGResource::kUnused)
            {
                setError("logical texture is not part of the current Bake");
                return false;
            }
            const uint32_t physicalIndex = resource->GetPhysicalIndex();
            if (physicalIndex >= m_PhysicalDims.size()
                || !MatchesExternalDesc(*texture, m_PhysicalDims[physicalIndex]))
            {
                setError("external texture does not match baked dimensions or usage");
                return false;
            }
        }

        cache->Externals[logicalName] = texture;
        return true;
    }

    void RenderGraph::UnregisterExternalTexture(const std::string& logicalName)
    {
        ViewPhysicalCache* cache = TryGetActiveViewCache();
        if (cache == nullptr || logicalName.empty())
        {
            return;
        }
        cache->Externals.erase(logicalName);

        // Drop the installed physical slot so the next SetupAttachments creates a graph-owned RT
        // instead of reusing the caller's texture by matching size/format.
        if (!m_IsBaked || m_BoundPhysicalTextures == nullptr)
        {
            return;
        }
        RDGTextureResource* resource = FindTextureResource(logicalName);
        if (resource == nullptr || resource->GetPhysicalIndex() == RDGResource::kUnused)
        {
            return;
        }
        const uint32_t physicalIndex = resource->GetPhysicalIndex();
        std::vector<std::shared_ptr<RHITexture>>& physicalTextures = *m_BoundPhysicalTextures;
        if (physicalIndex < physicalTextures.size())
        {
            physicalTextures[physicalIndex].reset();
        }
        cache->ExternalPhysicalIndices.erase(physicalIndex);
    }

    void RenderGraph::ClearExternalTextures()
    {
        ViewPhysicalCache* cache = TryGetActiveViewCache();
        if (cache == nullptr)
        {
            return;
        }
        if (m_IsBaked && m_BoundPhysicalTextures != nullptr)
        {
            std::vector<std::shared_ptr<RHITexture>>& physicalTextures = *m_BoundPhysicalTextures;
            for (uint32_t physicalIndex : cache->ExternalPhysicalIndices)
            {
                if (physicalIndex < physicalTextures.size())
                {
                    physicalTextures[physicalIndex].reset();
                }
            }
        }
        cache->Externals.clear();
        cache->ExternalPhysicalIndices.clear();
    }

    bool RenderGraph::MatchesExternalDesc(const RHITexture& texture, const RDGResourceDimensions& dims) const
    {
        const RHITextureCreateDesc& desc = texture.GetDesc();
        const RHITextureCreateDesc wantDesc = MakeCreateDesc(dims);
        return desc.Width == dims.Width && desc.Height == dims.Height && desc.Format == dims.Format
            && desc.DepthOrArrayLayers == dims.Layers
            && (desc.Flags & wantDesc.Flags) == wantDesc.Flags
            && desc.Dimension == dims.Dimension;
    }

    void RenderGraph::SetActiveViewKey(uint64_t viewId, const SceneRenderTarget* renderTarget)
    {
        const RDGViewKey key{viewId, renderTarget};
        if (!key.IsValid())
        {
            ME_LOG(LogRender, Error, "RenderGraph::SetActiveViewKey: ViewId must be non-zero.");
            m_BoundPhysicalTextures = nullptr;
            m_ActiveViewKey = {};
            return;
        }

        for (const ViewPhysicalCache& cache : m_ViewPhysicalCaches)
        {
            if (cache.Key.ViewId == key.ViewId && cache.Key.RenderTarget != key.RenderTarget)
            {
                ME_LOG(
                    LogRender,
                    Warn,
                    "RenderGraph: ViewId {} reused with a different SceneRenderTarget*; "
                    "fail-closed (new physical cache).",
                    key.ViewId);
            }
            else if (cache.Key.ViewId != key.ViewId && cache.Key.RenderTarget != nullptr
                && cache.Key.RenderTarget == key.RenderTarget)
            {
                ME_LOG(
                    LogRender,
                    Warn,
                    "RenderGraph: SceneRenderTarget* shared across ViewId {} and {}; "
                    "fail-closed (separate caches).",
                    cache.Key.ViewId,
                    key.ViewId);
            }
        }

        m_ActiveViewKey = key;
        BindPhysicalTexturesForActiveView();
    }

    RenderGraph::ViewPhysicalCache& RenderGraph::FindOrCreateViewCache(const RDGViewKey& key)
    {
        for (ViewPhysicalCache& cache : m_ViewPhysicalCaches)
        {
            if (cache.Key == key)
            {
                return cache;
            }
        }

        ViewPhysicalCache created{};
        created.Key = key;
        m_ViewPhysicalCaches.push_back(std::move(created));
        return m_ViewPhysicalCaches.back();
    }

    void RenderGraph::BindPhysicalTexturesForActiveView()
    {
        if (!m_ActiveViewKey.IsValid())
        {
            m_BoundPhysicalTextures = nullptr;
            return;
        }

        ViewPhysicalCache& cache = FindOrCreateViewCache(m_ActiveViewKey);
        m_BoundPhysicalTextures = &cache.Textures;
    }

    std::vector<std::shared_ptr<RHITexture>>& RenderGraph::PhysicalTextures()
    {
        if (m_BoundPhysicalTextures == nullptr)
        {
            throw std::logic_error(
                "RenderGraph: no active ViewKey; call SetActiveViewKey before Bake/SetupAttachments.");
        }
        return *m_BoundPhysicalTextures;
    }

    const std::vector<std::shared_ptr<RHITexture>>& RenderGraph::PhysicalTextures() const
    {
        if (m_BoundPhysicalTextures == nullptr)
        {
            throw std::logic_error(
                "RenderGraph: no active ViewKey; call SetActiveViewKey before Bake/SetupAttachments.");
        }
        return *m_BoundPhysicalTextures;
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
            throw std::logic_error("RenderGraph::Bake: InputRelative size is deferred.");
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
        // handle until SetupAttachments replaces mismatched slots (per active ViewKey cache).
        std::vector<std::shared_ptr<RHITexture>>& physicalTextures = PhysicalTextures();
        if (physicalTextures.size() > m_PhysicalDims.size())
        {
            physicalTextures.resize(m_PhysicalDims.size());
        }
        else if (physicalTextures.size() < m_PhysicalDims.size())
        {
            physicalTextures.resize(m_PhysicalDims.size(), nullptr);
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

    bool RenderGraph::SetupAttachments(RHI& rhi)
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

        std::vector<std::shared_ptr<RHITexture>>& physicalTextures = PhysicalTextures();
        if (physicalTextures.size() != m_PhysicalDims.size())
        {
            physicalTextures.assign(m_PhysicalDims.size(), nullptr);
        }

        ViewPhysicalCache* cache = TryGetActiveViewCache();
        std::unordered_set<uint32_t> previouslyExternal;
        std::unordered_map<uint32_t, RHITextureRef> externalByPhysical;
        if (cache != nullptr)
        {
            previouslyExternal = cache->ExternalPhysicalIndices;
            cache->ExternalPhysicalIndices.clear();
            for (const auto& [logicalName, texture] : cache->Externals)
            {
                RDGTextureResource* resource = FindTextureResource(logicalName);
                if (resource == nullptr || resource->GetPhysicalIndex() == RDGResource::kUnused)
                {
                    throw std::runtime_error(
                        "RenderGraph::SetupAttachments: external '" + logicalName
                        + "' is not part of the current Bake.");
                }
                const uint32_t physicalIndex = resource->GetPhysicalIndex();
                if (physicalIndex >= m_PhysicalDims.size())
                {
                    throw std::runtime_error(
                        "RenderGraph::SetupAttachments: external '" + logicalName
                        + "' has invalid physical index.");
                }
                if (!MatchesExternalDesc(*texture, m_PhysicalDims[physicalIndex]))
                {
                    ME_LOG(
                        LogRender,
                        Error,
                        "RenderGraph::SetupAttachments: external '{}' does not match baked slot '{}'.",
                        logicalName,
                        m_PhysicalDims[physicalIndex].DebugName);
                    throw std::runtime_error(
                        "RenderGraph::SetupAttachments: external texture mismatch for '" + logicalName
                        + "'.");
                }
                const auto existing = externalByPhysical.find(physicalIndex);
                if (existing != externalByPhysical.end() && existing->second != texture)
                {
                    throw std::runtime_error(
                        "RenderGraph::SetupAttachments: conflicting externals for the same physical slot.");
                }
                externalByPhysical[physicalIndex] = texture;
            }
        }

        bool anyRecreated = false;
        for (uint32_t physicalIndex = 0; physicalIndex < m_PhysicalDims.size(); ++physicalIndex)
        {
            const RDGResourceDimensions& dims = m_PhysicalDims[physicalIndex];
            std::shared_ptr<RHITexture>& slot = physicalTextures[physicalIndex];
            const RHITextureCreateDesc wantDesc = MakeCreateDesc(dims);

            const auto externalIt = externalByPhysical.find(physicalIndex);
            if (externalIt != externalByPhysical.end())
            {
                if (slot != externalIt->second)
                {
                    slot = externalIt->second;
                    anyRecreated = true;
                }
                if (cache != nullptr)
                {
                    cache->ExternalPhysicalIndices.insert(physicalIndex);
                }
                continue;
            }

            // A slot that was external last frame must not be reused as graph-owned after unbind.
            if (previouslyExternal.find(physicalIndex) != previouslyExternal.end())
            {
                slot.reset();
            }

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
            anyRecreated = true;
        }
        return anyRecreated;
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
        const std::vector<std::shared_ptr<RHITexture>>& physicalTextures = PhysicalTextures();
        if (physicalIndex == RDGResource::kUnused || physicalIndex >= physicalTextures.size())
        {
            throw std::logic_error("RenderGraph::GetPhysicalTexture: invalid physical index.");
        }
        RHITexture* texture = physicalTextures[physicalIndex].get();
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
        if (m_BoundPhysicalTextures == nullptr)
        {
            return nullptr;
        }
        const std::vector<std::shared_ptr<RHITexture>>& physicalTextures = PhysicalTextures();
        if (resource->GetPhysicalIndex() >= physicalTextures.size())
        {
            return nullptr;
        }
        return physicalTextures[resource->GetPhysicalIndex()].get();
    }

    RHITextureRef RenderGraph::GetPhysicalTextureShared(const RDGTextureResource& resource)
    {
        return GetPhysicalTextureShared(resource.GetPhysicalIndex());
    }

    RHITextureRef RenderGraph::GetPhysicalTextureShared(uint32_t physicalIndex)
    {
        const std::vector<std::shared_ptr<RHITexture>>& physicalTextures = PhysicalTextures();
        if (physicalIndex == RDGResource::kUnused || physicalIndex >= physicalTextures.size())
        {
            throw std::logic_error("RenderGraph::GetPhysicalTextureShared: invalid physical index.");
        }
        RHITextureRef texture = physicalTextures[physicalIndex];
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