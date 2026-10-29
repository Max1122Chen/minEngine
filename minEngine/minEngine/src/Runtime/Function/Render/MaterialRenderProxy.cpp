#include "MaterialRenderProxy.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/MaterialInstance.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <utility>
#include <vector>

namespace minEngine
{
    namespace
    {
        /**
         * Game-role read accounting, kept outside every proxy on purpose: a read made after the
         * material is gone still has to be observable.
         */
        struct GameObjectAccessCounters
        {
            std::atomic<bool> Enabled{ false };
            std::atomic<uint32_t> Total{ 0 };
            std::atomic<uint32_t> ParameterReads{ 0 };
            std::atomic<uint32_t> OwnerReads{ 0 };
            std::atomic<uint32_t> PublishReads{ 0 };
        };

        GameObjectAccessCounters& Counters()
        {
            static GameObjectAccessCounters counters;
            return counters;
        }

        void NoteRead(std::atomic<uint32_t>& bucket)
        {
            GameObjectAccessCounters& counters = Counters();
            if (!counters.Enabled.load(std::memory_order_relaxed))
            {
                return;
            }

            counters.Total.fetch_add(1, std::memory_order_relaxed);
            bucket.fetch_add(1, std::memory_order_relaxed);
        }

        RHISamplerDesc ResolveSamplerDesc(const MaterialRenderProxyTexture& parameter)
        {
            // The compile-time override wins over the texture's own preference, and a parameter with
            // no texture at all keeps the plain linear/repeat default the draw always fell back to.
            return parameter.bOverrideSampler ? parameter.SamplerIntent : parameter.TextureSampler;
        }

        MaterialRenderProxyScalar* FindMutableScalar(
            std::vector<MaterialRenderProxyScalar>& scalars, const std::string& name)
        {
            for (MaterialRenderProxyScalar& entry : scalars)
            {
                if (entry.Name == name)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        MaterialRenderProxyTexture* FindMutableTexture(
            std::vector<MaterialRenderProxyTexture>& textures, const std::string& name)
        {
            for (MaterialRenderProxyTexture& entry : textures)
            {
                if (entry.Name == name)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        /**
         * Live proxy registry, held weakly. Allocated once and never freed: proxies outlive static
         * destruction order at shutdown, and a proxy destructor must not touch a destroyed
         * container.
         */
        std::vector<std::weak_ptr<MaterialRenderProxy>>& Proxies()
        {
            static std::vector<std::weak_ptr<MaterialRenderProxy>>* proxies =
                new std::vector<std::weak_ptr<MaterialRenderProxy>>();
            return *proxies;
        }

        std::mutex& ProxiesMutex()
        {
            static std::mutex* mutex = new std::mutex();
            return *mutex;
        }

        bool IsGone(const std::weak_ptr<MaterialRenderProxy>& entry)
        {
            return entry.expired();
        }
    }

    MaterialRenderProxy::MaterialRenderProxy() = default;

    MaterialRenderProxy::~MaterialRenderProxy()
    {
        // Leaves the sweep behind the registry lock and touches nothing else, so the last
        // reference may be dropped on the role that happens to be collecting a frame.
        MaterialRenderProxyPublisher::Unregister(this);
    }

    MaterialRenderProxyDataRef MaterialRenderProxy::AcquirePublishedData() const
    {
        return std::atomic_load(&m_Published);
    }

    bool MaterialRenderProxy::IsCompiledForDraw() const
    {
        const MaterialRenderProxyDataRef data = AcquirePublishedData();
        if (data == nullptr || !data->bCompiledForDraw)
        {
            return false;
        }

        // The compile finished on the game role, but the program it produced is created by the
        // graphics role. Until that has run, the snapshot is a finished compile with nothing to
        // bind, which is exactly the state a draw has to skip.
        return data->Shader == nullptr || data->Shader->IsReady();
    }

    bool MaterialRenderProxy::IsCompiledForSkinnedDraw() const
    {
        const MaterialRenderProxyDataRef data = AcquirePublishedData();
        if (data == nullptr || !data->bCompiledForSkinnedDraw)
        {
            return false;
        }

        return data->SkinnedShader == nullptr || data->SkinnedShader->IsReady();
    }

    bool MaterialRenderProxy::IsTranslucent() const
    {
        const MaterialRenderProxyDataRef data = AcquirePublishedData();
        return data != nullptr && data->bTranslucent;
    }

    bool MaterialRenderProxy::IsMasked() const
    {
        const MaterialRenderProxyDataRef data = AcquirePublishedData();
        return data != nullptr && data->bMasked;
    }

    RHIShader* MaterialRenderProxy::GetGPUShader() const
    {
        const MaterialRenderProxyDataRef data = AcquirePublishedData();
        return data != nullptr ? data->Shader.get() : nullptr;
    }

    RHIShader* MaterialRenderProxy::GetGPUShaderSkinned() const
    {
        const MaterialRenderProxyDataRef data = AcquirePublishedData();
        return data != nullptr ? data->SkinnedShader.get() : nullptr;
    }

    RHIShaderBindingSetLayout* MaterialRenderProxy::GetMaterialShaderBindingSetLayout() const
    {
        const MaterialRenderProxyDataRef data = AcquirePublishedData();
        return data != nullptr ? data->BindingSetLayout.get() : nullptr;
    }

    RHIShaderBindingSet* MaterialRenderProxy::GetMaterialShaderBindingSet() const
    {
        return m_MaterialShaderBindingSet.get();
    }

    void MaterialRenderProxy::MarkGameDataDirty()
    {
        m_bGameDataDirty = true;
    }

    void MaterialRenderProxy::PublishGameData()
    {
        if (m_bOwnerDetached)
        {
            return;
        }

        NotePublishRead();

        std::shared_ptr<MaterialRenderProxyData> data = std::make_shared<MaterialRenderProxyData>();
        BuildGameData(*data);

        const MaterialRenderProxyDataRef previous = std::atomic_load(&m_Published);
        data->Serial = (previous != nullptr ? previous->Serial : 0u) + 1u;
        std::atomic_store(&m_Published, MaterialRenderProxyDataRef(data));

        ++m_PublishSerial;
        // A publish is a snapshot of the game-role tables, so any texture that changed its GPU
        // handle before this call is now accounted for.
        m_bGameDataDirty = false;
    }

    void MaterialRenderProxy::PublishPendingGameData()
    {
        if (m_bOwnerDetached)
        {
            return;
        }

        if (m_bGameDataDirty || HasStaleTextureReferences())
        {
            PublishGameData();
        }
    }

    void MaterialRenderProxy::DetachFromOwner()
    {
        m_bOwnerDetached = true;
        m_bGameDataDirty = false;
    }

    void MaterialRenderProxy::DropRenderState()
    {
        m_MaterialShaderBindingSet.reset();
        m_TextureSRVs.clear();
        m_BoundSamplers.clear();
        m_BoundTextureKeys.clear();
        m_BoundLayout = nullptr;
        m_BoundUniformBuffer = nullptr;
        m_ScalarParamsUBO.reset();
        m_ScalarParamsUBOSize = 0;
    }

    void MaterialRenderProxy::SyncScalarUniformBuffer(
        const MaterialRenderProxyData& data, RHICommandList& cmdList)
    {
        int maxSlot = -1;
        for (const MaterialRenderProxyScalar& scalar : data.Scalars)
        {
            maxSlot = std::max(maxSlot, scalar.SlotIndex);
        }

        if (maxSlot < 0)
        {
            m_ScalarParamsUBO.reset();
            m_ScalarParamsUBOSize = 0;
            return;
        }

        const uint32_t uboSize = static_cast<uint32_t>((maxSlot + 1) * 16u);
        if (!m_ScalarParamsUBO || m_ScalarParamsUBOSize != uboSize)
        {
            m_ScalarParamsUBOSize = uboSize;
            RHIBufferCreateDesc uboDesc;
            uboDesc.Usage = RHIBufferUsage::Uniform;
            uboDesc.ByteSize = m_ScalarParamsUBOSize;
            m_ScalarParamsUBO = cmdList.CreateBuffer(uboDesc, nullptr);
        }

        if (!m_ScalarParamsUBO)
        {
            return;
        }

        const uint32_t floatCount = m_ScalarParamsUBOSize / 16u;
        std::vector<float> scalarData(static_cast<size_t>(floatCount) * 4u, 0.0f);
        for (const MaterialRenderProxyScalar& scalar : data.Scalars)
        {
            if (scalar.SlotIndex < 0 || static_cast<uint32_t>(scalar.SlotIndex) >= floatCount)
            {
                continue;
            }

            scalarData[static_cast<size_t>(scalar.SlotIndex) * 4u] = scalar.Value;
        }

        cmdList.UpdateBuffer(m_ScalarParamsUBO.get(), scalarData.data(), 0, m_ScalarParamsUBOSize);
    }

    void MaterialRenderProxy::RebuildShaderBindingSet(
        const MaterialRenderProxyData& data, RHICommandList& cmdList)
    {
        RHIShaderBindingSetLayout* layout = data.BindingSetLayout.get();
        if (layout == nullptr)
        {
            m_MaterialShaderBindingSet.reset();
            m_TextureSRVs.clear();
            m_BoundSamplers.clear();
            m_BoundTextureKeys.clear();
            m_BoundLayout = nullptr;
            m_BoundUniformBuffer = nullptr;
            return;
        }

        // A snapshot that names the same layout, textures and samplers draws with the set that is
        // already built: a changed scalar only rewrites the uniform buffer.
        std::vector<BoundTextureKey> textureKeys;
        const std::vector<RHIShaderBindingSetLayoutEntry>& entries = layout->GetEntries();
        for (const RHIShaderBindingSetLayoutEntry& entry : entries)
        {
            if (entry.Type != RHIShaderBindingType::TextureSRV)
            {
                continue;
            }

            BoundTextureKey key;
            key.SlotIndex = static_cast<int>(entry.ShaderBinding);
            if (const MaterialRenderProxyTexture* parameter =
                    data.FindTextureBySlot(key.SlotIndex))
            {
                key.Texture = parameter->Texture.get();
                key.Sampler = ResolveSamplerDesc(*parameter);
            }
            textureKeys.push_back(key);
        }

        const bool sameTextures = textureKeys.size() == m_BoundTextureKeys.size()
            && std::equal(
                textureKeys.begin(),
                textureKeys.end(),
                m_BoundTextureKeys.begin(),
                [](const BoundTextureKey& left, const BoundTextureKey& right)
                {
                    return left.SlotIndex == right.SlotIndex && left.Texture == right.Texture
                        && left.Sampler == right.Sampler;
                });
        // The set also names the scalar uniform buffer by pointer: a grown or recreated buffer makes
        // the built set stale even when nothing else about the snapshot changed.
        if (m_MaterialShaderBindingSet && layout == m_BoundLayout && sameTextures
            && m_BoundUniformBuffer == m_ScalarParamsUBO.get())
        {
            return;
        }

        m_MaterialShaderBindingSet.reset();
        m_TextureSRVs.clear();
        m_BoundSamplers.clear();

        RHI* rhi = cmdList.GetExecutingRHI();
        std::vector<RHIShaderBinding> resources(entries.size());
        for (size_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex)
        {
            const RHIShaderBindingSetLayoutEntry& entry = entries[entryIndex];
            if (entry.Type == RHIShaderBindingType::UniformBuffer)
            {
                resources[entryIndex].Type = RHIShaderBindingType::UniformBuffer;
                resources[entryIndex].Buffer = m_ScalarParamsUBO.get();
                continue;
            }

            const MaterialRenderProxyTexture* parameter =
                data.FindTextureBySlot(static_cast<int>(entry.ShaderBinding));
            if (parameter == nullptr || !parameter->Texture)
            {
                continue;
            }

            RHIShaderResourceViewRef srv =
                m_TextureViewCache.GetOrCreate(cmdList, parameter->Texture.get());
            if (!srv)
            {
                continue;
            }
            m_TextureSRVs.push_back(srv);

            RHISamplerRef sampler;
            if (rhi != nullptr)
            {
                sampler = rhi->RHIGetOrCreateSampler(ResolveSamplerDesc(*parameter));
                if (sampler)
                {
                    m_BoundSamplers.push_back(sampler);
                }
            }

            resources[entryIndex].Type = RHIShaderBindingType::TextureSRV;
            resources[entryIndex].TextureSRV = srv.get();
            resources[entryIndex].Sampler = sampler.get();
        }

        m_MaterialShaderBindingSet = cmdList.CreateShaderBindingSet(layout, resources);
        m_BoundLayout = layout;
        m_BoundUniformBuffer = m_ScalarParamsUBO.get();
        m_BoundTextureKeys = std::move(textureKeys);
        ++m_BindingSetRebuildCount;
    }

    void MaterialRenderProxy::BindForDraw(RHICommandList& cmdList)
    {
        const MaterialRenderProxyDataRef data = AcquirePublishedData();
        if (!data)
        {
            return;
        }

        const bool generationChanged = data->CompileGeneration != m_SeenCompileGeneration;
        if (generationChanged)
        {
            // A compile replaced the layout this object built against: everything GPU-side is stale.
            DropRenderState();
            m_SeenCompileGeneration = data->CompileGeneration;
        }

        if (generationChanged || data->Serial != m_SeenSerial)
        {
            // Scalar UBO first: a binding set references it, so it has to exist when the set is built.
            SyncScalarUniformBuffer(*data, cmdList);
            RebuildShaderBindingSet(*data, cmdList);
            m_SeenSerial = data->Serial;
            return;
        }

        // Same snapshot: the binding set is still the one that was built from it.
        SyncScalarUniformBuffer(*data, cmdList);
    }

    void MaterialRenderProxy::SetGameObjectAccessProbe(bool bEnabled) const
    {
        GameObjectAccessCounters& counters = Counters();
        counters.Total.store(0, std::memory_order_relaxed);
        counters.ParameterReads.store(0, std::memory_order_relaxed);
        counters.OwnerReads.store(0, std::memory_order_relaxed);
        counters.PublishReads.store(0, std::memory_order_relaxed);
        counters.Enabled.store(bEnabled, std::memory_order_relaxed);
    }

    MaterialGameObjectAccess MaterialRenderProxy::ProbeGameObjectAccess() const
    {
        const GameObjectAccessCounters& counters = Counters();
        MaterialGameObjectAccess access;
        access.Total = counters.Total.load(std::memory_order_relaxed);
        access.ParameterReads = counters.ParameterReads.load(std::memory_order_relaxed);
        access.OwnerReads = counters.OwnerReads.load(std::memory_order_relaxed);
        access.PublishReads = counters.PublishReads.load(std::memory_order_relaxed);
        return access;
    }

    void MaterialRenderProxy::NoteParameterRead() const
    {
        NoteRead(Counters().ParameterReads);
    }

    void MaterialRenderProxy::NoteOwnerRead() const
    {
        NoteRead(Counters().OwnerReads);
    }

    void MaterialRenderProxy::NotePublishRead() const
    {
        NoteRead(Counters().PublishReads);
    }

    void MaterialRenderProxyPublisher::Register(const MaterialRenderProxyRef& proxy)
    {
        if (proxy == nullptr)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(ProxiesMutex());
        std::vector<std::weak_ptr<MaterialRenderProxy>>& proxies = Proxies();
        // A proxy released on another role leaves its entry behind: drop those before appending,
        // so a long session's proxy churn does not grow the registry with entries that can never
        // be published again.
        proxies.erase(std::remove_if(proxies.begin(), proxies.end(), IsGone), proxies.end());
        proxies.push_back(proxy);
    }

    void MaterialRenderProxyPublisher::Unregister(const MaterialRenderProxy* proxy)
    {
        std::lock_guard<std::mutex> lock(ProxiesMutex());
        std::vector<std::weak_ptr<MaterialRenderProxy>>& proxies = Proxies();
        proxies.erase(
            std::remove_if(
                proxies.begin(),
                proxies.end(),
                [proxy](const std::weak_ptr<MaterialRenderProxy>& entry)
                {
                    // An entry for the object being destroyed no longer locks: the registry refers
                    // to it weakly, and its own reference count is already zero at this point.
                    const MaterialRenderProxyRef held = entry.lock();
                    return !held || held.get() == proxy;
                }),
            proxies.end());
    }

    void MaterialRenderProxyPublisher::PublishPendingGameData()
    {
        // The sweep reads game-role tables for every material that changed. Publishing them from
        // any other role would hand the render role a snapshot built from memory it may not read.
        if (FrameThreading::HasInstance() && !FrameThreading::Get().IsInGameThread())
        {
            static std::atomic<bool> reportedOffRole{ false };
            if (!reportedOffRole.exchange(true))
            {
                ME_LOG(
                    LogCore,
                    Error,
                    "MaterialRenderProxyPublisher: publishing off the game role; material parameter "
                    "changes are game-role state");
            }
        }

        // The lock keeps the registry still while it is walked. A publish never registers,
        // unregisters or publishes anything that could take this lock again - an instance reaches
        // its parent proxy, which is not a registry operation - so waiting here cannot deadlock.
        // Register and unregister are rare, so they merely wait.
        std::lock_guard<std::mutex> lock(ProxiesMutex());
        for (const std::weak_ptr<MaterialRenderProxy>& entry : Proxies())
        {
            // Pinning it first is what makes the publish safe against a proxy released on another
            // role: destruction starts only once no reference is left, which is exactly when this
            // lock fails and the sweep skips the entry.
            const MaterialRenderProxyRef proxy = entry.lock();
            if (proxy != nullptr)
            {
                proxy->PublishPendingGameData();
            }
        }
    }

    size_t MaterialRenderProxyPublisher::GetRegisteredCount()
    {
        std::lock_guard<std::mutex> lock(ProxiesMutex());
        size_t liveCount = 0;
        for (const std::weak_ptr<MaterialRenderProxy>& entry : Proxies())
        {
            if (!entry.expired())
            {
                ++liveCount;
            }
        }
        return liveCount;
    }

    DefaultMaterialRenderProxy::DefaultMaterialRenderProxy(Material& material)
        : m_Material(&material)
    {
    }

    Material* DefaultMaterialRenderProxy::GetBaseMaterial() const
    {
        NoteOwnerRead();
        return m_Material;
    }

    void DefaultMaterialRenderProxy::DetachFromOwner()
    {
        // Dropped here rather than read defensively everywhere: after this point the proxy has no
        // owner, and every remaining reader draws from the snapshot it published.
        m_Material = nullptr;
        MaterialRenderProxy::DetachFromOwner();
    }

    void DefaultMaterialRenderProxy::BuildGameData(MaterialRenderProxyData& outData) const
    {
        const Material* material = m_Material;
        if (material == nullptr)
        {
            return;
        }

        outData.CompileGeneration = material->m_CompileGeneration;
        // "Compiled" is a compile-time fact: a program handle and a parameter layout. Whether the
        // handle's program exists yet is asked of the handle, because this snapshot is taken on
        // the game role and cannot observe the graphics role creating it afterwards.
        outData.bCompiledForDraw = material->m_GPUShader != nullptr
            && !material->m_ParameterLayout.Parameters.empty();
        outData.bCompiledForSkinnedDraw = outData.bCompiledForDraw
            && material->m_GPUShaderSkinned != nullptr;
        outData.bTranslucent = material->m_BlendMode == MaterialBlendMode::Translucent;
        outData.bMasked = material->m_BlendMode == MaterialBlendMode::Masked;

        outData.Shader = material->m_GPUShader;
        outData.SkinnedShader = material->m_GPUShaderSkinned;
        outData.BindingSetLayout = material->m_MaterialShaderBindingSetLayout;

        outData.Scalars.reserve(material->m_ScalarParameters.size());
        for (const MaterialScalarParameter& parameter : material->m_ScalarParameters)
        {
            MaterialRenderProxyScalar scalar;
            scalar.Name = parameter.ParameterName;
            scalar.SlotIndex = parameter.SlotIndex;
            scalar.Value = parameter.Value;
            outData.Scalars.push_back(std::move(scalar));
        }

        outData.Textures.reserve(material->m_TextureParameters.size());
        for (const MaterialTextureParameter& parameter : material->m_TextureParameters)
        {
            MaterialRenderProxyTexture texture;
            texture.Name = parameter.ParameterName;
            texture.SlotIndex = parameter.SlotIndex;
            texture.Texture =
                parameter.Value ? parameter.Value->GetRHITextureShared() : nullptr;
            texture.bOverrideSampler = parameter.bOverrideSampler;
            texture.SamplerIntent = parameter.SamplerDesc;
            texture.TextureSampler = parameter.Value
                ? parameter.Value->GetPreferredSamplerDesc()
                : MakeLinearRepeatSamplerDesc();
            outData.Textures.push_back(std::move(texture));
        }
    }

    bool DefaultMaterialRenderProxy::HasStaleTextureReferences() const
    {
        const Material* material = m_Material;
        if (material == nullptr || material->m_TextureParameters.empty())
        {
            return false;
        }

        const MaterialRenderProxyDataRef published = AcquirePublishedData();
        if (!published)
        {
            return true;
        }

        for (const MaterialTextureParameter& parameter : material->m_TextureParameters)
        {
            const MaterialRenderProxyTexture* existing =
                published->FindTextureBySlot(parameter.SlotIndex);
            if (existing == nullptr)
            {
                return true;
            }

            // A texture that finishes loading or changes its sampler after the last publish would
            // otherwise stay invisible until the next parameter write.
            const RHITextureRef textureRef =
                parameter.Value ? parameter.Value->GetRHITextureShared() : nullptr;
            if (existing->Texture.get() != textureRef.get())
            {
                return true;
            }

            if (parameter.Value
                && existing->TextureSampler != parameter.Value->GetPreferredSamplerDesc())
            {
                return true;
            }
        }

        return false;
    }

    bool DefaultMaterialRenderProxy::GetScalarParameterValue(
        const std::string& name, float& outValue) const
    {
        NoteParameterRead();
        if (m_Material == nullptr)
        {
            return false;
        }

        for (const MaterialScalarParameter& parameter : m_Material->m_ScalarParameters)
        {
            if (parameter.ParameterName == name)
            {
                outValue = parameter.Value;
                return true;
            }
        }
        return false;
    }

    bool DefaultMaterialRenderProxy::GetTextureParameterValue(
        const std::string& name, std::shared_ptr<Texture2D>& outTexture) const
    {
        NoteParameterRead();
        if (m_Material == nullptr)
        {
            return false;
        }

        for (const MaterialTextureParameter& parameter : m_Material->m_TextureParameters)
        {
            if (parameter.ParameterName == name)
            {
                outTexture = parameter.Value;
                return true;
            }
        }
        return false;
    }

    bool DefaultMaterialRenderProxy::GetScalarParameterValueBySlot(
        int slotIndex, float& outValue) const
    {
        NoteParameterRead();
        if (m_Material == nullptr)
        {
            return false;
        }

        for (const MaterialScalarParameter& parameter : m_Material->m_ScalarParameters)
        {
            if (parameter.SlotIndex == slotIndex)
            {
                outValue = parameter.Value;
                return true;
            }
        }
        return false;
    }

    bool DefaultMaterialRenderProxy::GetTextureParameterValueBySlot(
        int slotIndex, std::shared_ptr<Texture2D>& outTexture) const
    {
        NoteParameterRead();
        if (m_Material == nullptr)
        {
            return false;
        }

        for (const MaterialTextureParameter& parameter : m_Material->m_TextureParameters)
        {
            if (parameter.SlotIndex == slotIndex)
            {
                outTexture = parameter.Value;
                return true;
            }
        }
        return false;
    }

    InstanceMaterialRenderProxy::InstanceMaterialRenderProxy(MaterialInstance& instance)
        : m_Instance(&instance)
    {
    }

    void InstanceMaterialRenderProxy::SetParentProxy(MaterialRenderProxy* parentProxy)
    {
        m_ParentProxy = parentProxy;
    }

    MaterialRenderProxy* InstanceMaterialRenderProxy::ResolveParentProxy() const
    {
        if (m_Instance != nullptr && m_Instance->m_Parent)
        {
            // Game role only: this is where the parent *proxy* is picked up, once, so a draw never
            // has to walk from the instance to the material that owns it.
            MaterialRenderProxy* parentProxy = m_Instance->m_Parent->GetRenderProxy();
            if (parentProxy != nullptr)
            {
                m_ParentProxy = parentProxy;
            }
        }

        return m_ParentProxy;
    }

    Material* InstanceMaterialRenderProxy::GetBaseMaterial() const
    {
        NoteOwnerRead();
        return m_Instance ? m_Instance->GetBaseMaterial() : nullptr;
    }

    void InstanceMaterialRenderProxy::PublishPendingGameData()
    {
        if (IsOwnerDetached())
        {
            return;
        }

        MaterialRenderProxy* parentProxy = ResolveParentProxy();
        if (parentProxy != nullptr)
        {
            // The parent publishes first: this snapshot copies its slots and values.
            parentProxy->PublishPendingGameData();
        }

        const uint64_t parentSerial = parentProxy != nullptr ? parentProxy->GetPublishSerial() : 0u;
        if (m_bGameDataDirty || parentSerial != m_SeenParentPublishSerial
            || HasStaleTextureReferences())
        {
            PublishGameData();
            m_SeenParentPublishSerial = parentSerial;
        }
    }

    void InstanceMaterialRenderProxy::DetachFromOwner()
    {
        // The instance and the parent proxy are both game-role links: nothing reads either one
        // after this point, and the flattened snapshot stays as the last word on what this draws.
        m_Instance = nullptr;
        m_ParentProxy = nullptr;
        MaterialRenderProxy::DetachFromOwner();
    }

    void InstanceMaterialRenderProxy::BuildGameData(MaterialRenderProxyData& outData) const
    {
        MaterialRenderProxy* parentProxy = ResolveParentProxy();
        const MaterialRenderProxyDataRef parentData =
            parentProxy != nullptr ? parentProxy->AcquirePublishedData() : nullptr;
        if (parentData != nullptr)
        {
            // Flattened here so the draw side has one table: the parent's slots and values, with
            // this instance's overrides applied on top of them.
            outData.CompileGeneration = parentData->CompileGeneration;
            outData.bCompiledForDraw = parentData->bCompiledForDraw;
            outData.bCompiledForSkinnedDraw = parentData->bCompiledForSkinnedDraw;
            outData.bTranslucent = parentData->bTranslucent;
            outData.bMasked = parentData->bMasked;
            outData.Shader = parentData->Shader;
            outData.SkinnedShader = parentData->SkinnedShader;
            outData.BindingSetLayout = parentData->BindingSetLayout;
            outData.Scalars = parentData->Scalars;
            outData.Textures = parentData->Textures;
        }

        const MaterialInstance* instance = m_Instance;
        if (instance == nullptr)
        {
            return;
        }

        for (const MaterialScalarOverrideEntry& override : instance->m_ScalarOverrides)
        {
            if (MaterialRenderProxyScalar* existing =
                    FindMutableScalar(outData.Scalars, override.ParameterName))
            {
                existing->Value = override.Value;
                continue;
            }

            MaterialRenderProxyScalar scalar;
            scalar.Name = override.ParameterName;
            scalar.Value = override.Value;
            outData.Scalars.push_back(std::move(scalar));
        }

        for (const MaterialTextureOverrideEntry& override : instance->m_TextureOverrides)
        {
            const RHITextureRef textureRef =
                override.Value ? override.Value->GetRHITextureShared() : nullptr;
            const RHISamplerDesc textureSampler = override.Value
                ? override.Value->GetPreferredSamplerDesc()
                : MakeLinearRepeatSamplerDesc();

            if (MaterialRenderProxyTexture* existing =
                    FindMutableTexture(outData.Textures, override.ParameterName))
            {
                existing->Texture = textureRef;
                existing->TextureSampler = textureSampler;
                continue;
            }

            MaterialRenderProxyTexture texture;
            texture.Name = override.ParameterName;
            texture.Texture = textureRef;
            texture.TextureSampler = textureSampler;
            outData.Textures.push_back(std::move(texture));
        }
    }

    bool InstanceMaterialRenderProxy::HasStaleTextureReferences() const
    {
        if (m_Instance == nullptr || m_Instance->m_TextureOverrides.empty())
        {
            return false;
        }

        const MaterialRenderProxyDataRef published = AcquirePublishedData();
        if (!published)
        {
            return true;
        }

        for (const MaterialTextureOverrideEntry& override : m_Instance->m_TextureOverrides)
        {
            const MaterialRenderProxyTexture* existing =
                published->FindTextureByName(override.ParameterName);
            if (existing == nullptr)
            {
                return true;
            }

            const RHITextureRef textureRef =
                override.Value ? override.Value->GetRHITextureShared() : nullptr;
            if (existing->Texture.get() != textureRef.get())
            {
                return true;
            }

            if (override.Value
                && existing->TextureSampler != override.Value->GetPreferredSamplerDesc())
            {
                return true;
            }
        }

        return false;
    }

    bool InstanceMaterialRenderProxy::GetScalarParameterValue(
        const std::string& name, float& outValue) const
    {
        NoteParameterRead();
        if (m_Instance != nullptr && m_Instance->TryGetScalarOverride(name, outValue))
        {
            return true;
        }

        const MaterialRenderProxy* parentProxy = ResolveParentProxy();
        return parentProxy != nullptr && parentProxy->GetScalarParameterValue(name, outValue);
    }

    bool InstanceMaterialRenderProxy::GetTextureParameterValue(
        const std::string& name, std::shared_ptr<Texture2D>& outTexture) const
    {
        NoteParameterRead();
        if (m_Instance != nullptr && m_Instance->TryGetTextureOverride(name, outTexture))
        {
            return true;
        }

        const MaterialRenderProxy* parentProxy = ResolveParentProxy();
        return parentProxy != nullptr && parentProxy->GetTextureParameterValue(name, outTexture);
    }

    bool InstanceMaterialRenderProxy::GetScalarParameterValueBySlot(
        int slotIndex, float& outValue) const
    {
        NoteParameterRead();
        const Material* base = GetBaseMaterial();
        if (m_Instance != nullptr && base != nullptr)
        {
            for (const MaterialScalarParameter& parameter : base->m_ScalarParameters)
            {
                if (parameter.SlotIndex != slotIndex)
                {
                    continue;
                }

                if (m_Instance->TryGetScalarOverride(parameter.ParameterName, outValue))
                {
                    return true;
                }
                break;
            }
        }

        const MaterialRenderProxy* parentProxy = ResolveParentProxy();
        return parentProxy != nullptr && parentProxy->GetScalarParameterValueBySlot(slotIndex, outValue);
    }

    bool InstanceMaterialRenderProxy::GetTextureParameterValueBySlot(
        int slotIndex, std::shared_ptr<Texture2D>& outTexture) const
    {
        NoteParameterRead();
        const Material* base = GetBaseMaterial();
        if (m_Instance != nullptr && base != nullptr)
        {
            for (const MaterialTextureParameter& parameter : base->m_TextureParameters)
            {
                if (parameter.SlotIndex != slotIndex)
                {
                    continue;
                }

                if (m_Instance->TryGetTextureOverride(parameter.ParameterName, outTexture))
                {
                    return true;
                }
                break;
            }
        }

        const MaterialRenderProxy* parentProxy = ResolveParentProxy();
        return parentProxy != nullptr
            && parentProxy->GetTextureParameterValueBySlot(slotIndex, outTexture);
    }
}