#include "EngineSceneBindingSets.h"

#include "EngineShaderBindings.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

namespace minEngine
{
    namespace
    {
        using namespace EngineShaderBindings;
    }

    void EngineSceneBindingSets::Initialize(RHICommandList& cmdList)
    {
        // PerFrame is read by both stages (VS transforms + FS CameraPos in lit templates).
        // Vulkan requires stageFlags to cover every stage that declares the binding.
        m_SceneSet0Layout = cmdList.CreateShaderBindingSetLayout({
            {kSet0_PerFrame, RHIShaderBindingType::UniformBuffer, kGL_PerFrameUBO, RHIGraphicsShaderStage::All},
            {kSet0_Lights, RHIShaderBindingType::UniformBuffer, kGL_LightsUBO, RHIGraphicsShaderStage::Pixel},
            {kSet0_PerObject, RHIShaderBindingType::UniformBuffer, kGL_PerObjectUBO, RHIGraphicsShaderStage::Vertex},
        });

        m_SceneSet1Layout = cmdList.CreateShaderBindingSetLayout({
            {kSet1_DirShadowSRV, RHIShaderBindingType::TextureSRV, kGL_DirShadowTextureUnit, RHIGraphicsShaderStage::Pixel},
            {kSet1_DirLightViewProjs, RHIShaderBindingType::UniformBuffer, kGL_DirLightViewProjsUBO, RHIGraphicsShaderStage::Pixel},
            {kSet1_CascadeFarPlanes, RHIShaderBindingType::UniformBuffer, kGL_CascadeFarPlanesUBO, RHIGraphicsShaderStage::Pixel},
            {kSet1_SpotLightViewProjs, RHIShaderBindingType::UniformBuffer, kGL_SpotLightViewProjsUBO, RHIGraphicsShaderStage::Pixel},
            {kSet1_SpotShadow0, RHIShaderBindingType::TextureSRV, kGL_SpotShadowBaseUnit, RHIGraphicsShaderStage::Pixel},
            {kSet1_SpotShadow1, RHIShaderBindingType::TextureSRV, kGL_SpotShadowBaseUnit + 1, RHIGraphicsShaderStage::Pixel},
            {kSet1_PointShadow0, RHIShaderBindingType::TextureSRV, kGL_PointShadowBaseUnit, RHIGraphicsShaderStage::Pixel},
            {kSet1_PointShadow1, RHIShaderBindingType::TextureSRV, kGL_PointShadowBaseUnit + 1, RHIGraphicsShaderStage::Pixel},
            {kSet1_IBLIrradiance, RHIShaderBindingType::TextureSRV, kGL_IBLIrradianceUnit, RHIGraphicsShaderStage::Pixel},
            {kSet1_IBLPrefilter, RHIShaderBindingType::TextureSRV, kGL_IBLPrefilterUnit, RHIGraphicsShaderStage::Pixel},
            {kSet1_IBLBrdfLut, RHIShaderBindingType::TextureSRV, kGL_IBLBrdfLutUnit, RHIGraphicsShaderStage::Pixel},
        });
    }

    void EngineSceneBindingSets::Shutdown()
    {
        m_SceneSet0BySlot.clear();
        m_SceneSet1.reset();
        m_SceneSet0Layout.reset();
        m_SceneSet1Layout.reset();
        m_DirShadowSRV.reset();
        m_SpotShadowSRVs = {};
        m_PointShadowSRVs = {};
        m_IblSRVs = {};
        m_TextureViewCache.Clear();
        m_CachedPerFrame = nullptr;
        m_CachedLights = nullptr;
        m_PerObjectRing = nullptr;
        m_PerObjectWriteIndex = 0;
        m_CachedDirShadowTexture = nullptr;
        m_CachedSpotShadowTextures = {};
        m_CachedPointShadowTextures = {};
        m_CachedDirLightViewProjs = nullptr;
        m_CachedCascadeFarPlanes = nullptr;
        m_CachedSpotLightViewProjs = nullptr;
        m_CachedIblIrradianceTexture = nullptr;
        m_CachedIblPrefilterTexture = nullptr;
        m_CachedIblBrdfLutTexture = nullptr;
        m_ShadowBindingGeneration = 0;
        m_BuiltShadowBindingGeneration = UINT32_MAX;
    }

    RHIShaderResourceViewRef EngineSceneBindingSets::GetOrCreateTextureSRV(
        RHICommandList& cmdList,
        RHITexture* texture,
        int32_t arraySlice)
    {
        return m_TextureViewCache.GetOrCreate(cmdList, texture, arraySlice);
    }

    void EngineSceneBindingSets::BeginFrame(
        RHICommandList& cmdList,
        RHIBuffer* perFrame,
        RHIBuffer* lights,
        RHIBuffer* perObjectRing,
        uint32_t perObjectSlotStride)
    {
        (void)cmdList;
        const bool ringIdentityChanged =
            perFrame != m_CachedPerFrame
            || lights != m_CachedLights
            || perObjectRing != m_PerObjectRing
            || perObjectSlotStride != m_PerObjectSlotStride;

        m_CachedPerFrame = perFrame;
        m_CachedLights = lights;
        m_PerObjectRing = perObjectRing;
        m_PerObjectSlotStride = perObjectSlotStride == 0 ? 256u : perObjectSlotStride;
        m_PerObjectWriteIndex = 0;

        if (ringIdentityChanged)
        {
            m_SceneSet0BySlot.clear();
            m_SceneSet0BySlot.resize(kPerObjectRingSlots);
        }
    }

    uint32_t EngineSceneBindingSets::WriteNextPerObjectModel(const Matrix4& model)
    {
        if (!m_PerObjectRing)
        {
            return 0;
        }

        if (m_PerObjectWriteIndex >= kPerObjectRingSlots)
        {
            ME_CORE_ERROR(
                "EngineSceneBindingSets: Per-Object UBO ring exhausted ({} slots). "
                "Wrapping — later draws may corrupt earlier in-flight matrices.",
                kPerObjectRingSlots);
            m_PerObjectWriteIndex = 0;
        }

        const uint32_t slot = m_PerObjectWriteIndex++;
        const uint32_t offset = slot * m_PerObjectSlotStride;
        m_PerObjectRing->UpdateSubresource(&model, offset, sizeof(Matrix4));
        return offset;
    }

    RHIShaderBindingSet* EngineSceneBindingSets::BindNextPerObjectModel(
        RHICommandList& cmdList,
        const Matrix4& model)
    {
        if (!m_SceneSet0Layout || !m_CachedPerFrame || !m_CachedLights || !m_PerObjectRing)
        {
            return nullptr;
        }

        const uint32_t slotBefore = m_PerObjectWriteIndex;
        const uint32_t offset = WriteNextPerObjectModel(model);
        const uint32_t slot = slotBefore % kPerObjectRingSlots;

        if (slot >= m_SceneSet0BySlot.size())
        {
            m_SceneSet0BySlot.resize(kPerObjectRingSlots);
        }

        if (!m_SceneSet0BySlot[slot])
        {
            std::vector<RHIShaderBinding> resources(3);
            resources[kSet0_PerFrame] = {RHIShaderBindingType::UniformBuffer, m_CachedPerFrame, nullptr, 0, 0};
            resources[kSet0_Lights] = {RHIShaderBindingType::UniformBuffer, m_CachedLights, nullptr, 0, 0};
            resources[kSet0_PerObject] = {
                RHIShaderBindingType::UniformBuffer,
                m_PerObjectRing,
                nullptr,
                offset,
                static_cast<uint32_t>(sizeof(Matrix4))};
            m_SceneSet0BySlot[slot] = cmdList.CreateShaderBindingSet(m_SceneSet0Layout.get(), resources);
        }

        return m_SceneSet0BySlot[slot].get();
    }

    void EngineSceneBindingSets::InvalidateShadowTextureBindings()
    {
        ++m_ShadowBindingGeneration;
        m_CachedDirShadowTexture = nullptr;
        m_CachedSpotShadowTextures.fill(nullptr);
        m_CachedPointShadowTextures.fill(nullptr);
        m_DirShadowSRV.reset();
        m_SpotShadowSRVs.fill(nullptr);
        m_PointShadowSRVs.fill(nullptr);
        m_TextureViewCache.Clear();
    }

    void EngineSceneBindingSets::BuildSceneSet1(
        RHICommandList& cmdList,
        const SceneRenderContext& ctx,
        RHIBuffer* dirLightViewProjs,
        RHIBuffer* cascadeFarPlanes,
        RHIBuffer* spotLightViewProjs)
    {
        if (!m_SceneSet1Layout)
        {
            return;
        }

        bool sceneSet1Dirty =
            !m_SceneSet1 || m_ShadowBindingGeneration != m_BuiltShadowBindingGeneration;

        RHITexture* dirShadowTexture = nullptr;
        if (ctx.DirectionalShadowHandle.IsValid())
        {
            dirShadowTexture = ctx.DirectionalShadowHandle.Texture.get();
        }
        if (dirShadowTexture != m_CachedDirShadowTexture)
        {
            m_CachedDirShadowTexture = dirShadowTexture;
            m_DirShadowSRV = dirShadowTexture ? GetOrCreateTextureSRV(cmdList, dirShadowTexture) : nullptr;
            sceneSet1Dirty = true;
        }

        for (size_t i = 0; i < MAX_SPOT_SHADOW_MAPS; ++i)
        {
            RHITexture* spotTexture = nullptr;
            if (i < ctx.SpotShadowHandles.size())
            {
                const ShadowResourceHandle& handle = ctx.SpotShadowHandles[i];
                if (handle.IsValid())
                {
                    spotTexture = handle.Texture.get();
                }
            }

            if (spotTexture != m_CachedSpotShadowTextures[i])
            {
                m_CachedSpotShadowTextures[i] = spotTexture;
                m_SpotShadowSRVs[i] = spotTexture ? GetOrCreateTextureSRV(cmdList, spotTexture) : nullptr;
                sceneSet1Dirty = true;
            }
        }

        for (size_t i = 0; i < MAX_POINT_SHADOW_MAPS; ++i)
        {
            RHITexture* pointTexture = nullptr;
            if (i < ctx.PointShadowHandles.size())
            {
                const ShadowResourceHandle& handle = ctx.PointShadowHandles[i];
                if (handle.IsValid())
                {
                    pointTexture = handle.Texture.get();
                }
            }

            if (pointTexture != m_CachedPointShadowTextures[i])
            {
                m_CachedPointShadowTextures[i] = pointTexture;
                m_PointShadowSRVs[i] = pointTexture ? GetOrCreateTextureSRV(cmdList, pointTexture) : nullptr;
                sceneSet1Dirty = true;
            }
        }

        if (dirLightViewProjs != m_CachedDirLightViewProjs)
        {
            m_CachedDirLightViewProjs = dirLightViewProjs;
            sceneSet1Dirty = true;
        }
        if (cascadeFarPlanes != m_CachedCascadeFarPlanes)
        {
            m_CachedCascadeFarPlanes = cascadeFarPlanes;
            sceneSet1Dirty = true;
        }
        if (spotLightViewProjs != m_CachedSpotLightViewProjs)
        {
            m_CachedSpotLightViewProjs = spotLightViewProjs;
            sceneSet1Dirty = true;
        }

        auto refreshIblSrv = [&](RHITexture* texture, RHITexture*& cachedTexture, RHIShaderResourceViewRef& srv)
        {
            if (texture != cachedTexture)
            {
                cachedTexture = texture;
                srv = texture ? GetOrCreateTextureSRV(cmdList, texture) : nullptr;
                sceneSet1Dirty = true;
            }
        };

        refreshIblSrv(ctx.IblIrradianceTexture, m_CachedIblIrradianceTexture, m_IblSRVs[0]);
        refreshIblSrv(ctx.IblPrefilterTexture, m_CachedIblPrefilterTexture, m_IblSRVs[1]);
        refreshIblSrv(ctx.IblBrdfLutTexture, m_CachedIblBrdfLutTexture, m_IblSRVs[2]);

        if (!sceneSet1Dirty)
        {
            return;
        }

        std::vector<RHIShaderBinding> resources(11);
        resources[kSet1_DirShadowSRV] = {RHIShaderBindingType::TextureSRV, nullptr, m_DirShadowSRV.get()};
        resources[kSet1_DirLightViewProjs] = {RHIShaderBindingType::UniformBuffer, dirLightViewProjs, nullptr};
        resources[kSet1_CascadeFarPlanes] = {RHIShaderBindingType::UniformBuffer, cascadeFarPlanes, nullptr};
        resources[kSet1_SpotLightViewProjs] = {RHIShaderBindingType::UniformBuffer, spotLightViewProjs, nullptr};
        resources[kSet1_SpotShadow0] = {RHIShaderBindingType::TextureSRV, nullptr, m_SpotShadowSRVs[0].get()};
        resources[kSet1_SpotShadow1] = {RHIShaderBindingType::TextureSRV, nullptr, m_SpotShadowSRVs[1].get()};
        resources[kSet1_PointShadow0] = {RHIShaderBindingType::TextureSRV, nullptr, m_PointShadowSRVs[0].get()};
        resources[kSet1_PointShadow1] = {RHIShaderBindingType::TextureSRV, nullptr, m_PointShadowSRVs[1].get()};
        resources[kSet1_IBLIrradiance] = {RHIShaderBindingType::TextureSRV, nullptr, m_IblSRVs[0].get()};
        resources[kSet1_IBLPrefilter] = {RHIShaderBindingType::TextureSRV, nullptr, m_IblSRVs[1].get()};
        resources[kSet1_IBLBrdfLut] = {RHIShaderBindingType::TextureSRV, nullptr, m_IblSRVs[2].get()};

        RHIShaderBindingSetRef newSceneSet1 =
            cmdList.CreateShaderBindingSet(m_SceneSet1Layout.get(), resources);
        if (newSceneSet1)
        {
            m_SceneSet1 = std::move(newSceneSet1);
            m_BuiltShadowBindingGeneration = m_ShadowBindingGeneration;
        }
        else
        {
            ME_CORE_ERROR("EngineSceneBindingSets: failed to create scene set 1 (shadow/IBL bindings).");
        }
    }
}
