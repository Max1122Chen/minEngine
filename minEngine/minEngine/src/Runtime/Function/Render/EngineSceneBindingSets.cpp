#include "EngineSceneBindingSets.h"

#include "EngineShaderBindings.h"
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
        m_SceneSet0Layout = cmdList.CreateShaderBindingSetLayout({
            {kSet0_PerFrame, RHIShaderBindingType::UniformBuffer, kGL_PerFrameUBO, RHIGraphicsShaderStage::Vertex},
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
        m_SceneSet0.reset();
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
        m_CachedPerObject = nullptr;
        m_CachedDirShadowTexture = nullptr;
        m_CachedSpotShadowTextures = {};
        m_CachedPointShadowTextures = {};
        m_CachedDirLightViewProjs = nullptr;
        m_CachedCascadeFarPlanes = nullptr;
        m_CachedSpotLightViewProjs = nullptr;
        m_CachedIblIrradianceTexture = nullptr;
        m_CachedIblPrefilterTexture = nullptr;
        m_CachedIblBrdfLutTexture = nullptr;
    }

    RHIShaderResourceViewRef EngineSceneBindingSets::GetOrCreateTextureSRV(
        RHICommandList& cmdList,
        RHITexture* texture,
        int32_t arraySlice)
    {
        return m_TextureViewCache.GetOrCreate(cmdList, texture, arraySlice);
    }

    void EngineSceneBindingSets::BuildSceneSet0(
        RHICommandList& cmdList,
        RHIBuffer* perFrame,
        RHIBuffer* lights,
        RHIBuffer* perObject)
    {
        if (!m_SceneSet0Layout)
        {
            return;
        }

        const bool sceneSet0Dirty =
            !m_SceneSet0
            || perFrame != m_CachedPerFrame
            || lights != m_CachedLights
            || perObject != m_CachedPerObject;
        if (!sceneSet0Dirty)
        {
            return;
        }

        m_CachedPerFrame = perFrame;
        m_CachedLights = lights;
        m_CachedPerObject = perObject;

        std::vector<RHIShaderBinding> resources(3);
        resources[kSet0_PerFrame] = {RHIShaderBindingType::UniformBuffer, perFrame, nullptr};
        resources[kSet0_Lights] = {RHIShaderBindingType::UniformBuffer, lights, nullptr};
        resources[kSet0_PerObject] = {RHIShaderBindingType::UniformBuffer, perObject, nullptr};
        m_SceneSet0 = cmdList.CreateShaderBindingSet(m_SceneSet0Layout.get(), resources);
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

        bool sceneSet1Dirty = !m_SceneSet1;

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

        for (size_t i = 0; i < ctx.SpotShadowHandles.size() && i < m_SpotShadowSRVs.size(); ++i)
        {
            RHITexture* spotTexture = nullptr;
            const ShadowResourceHandle& handle = ctx.SpotShadowHandles[i];
            if (handle.IsValid())
            {
                spotTexture = handle.Texture.get();
            }

            if (spotTexture != m_CachedSpotShadowTextures[i])
            {
                m_CachedSpotShadowTextures[i] = spotTexture;
                m_SpotShadowSRVs[i] = spotTexture ? GetOrCreateTextureSRV(cmdList, spotTexture) : nullptr;
                sceneSet1Dirty = true;
            }
        }

        for (size_t i = 0; i < ctx.PointShadowHandles.size() && i < m_PointShadowSRVs.size(); ++i)
        {
            RHITexture* pointTexture = nullptr;
            const ShadowResourceHandle& handle = ctx.PointShadowHandles[i];
            if (handle.IsValid())
            {
                pointTexture = handle.Texture.get();
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

        m_SceneSet1 = cmdList.CreateShaderBindingSet(m_SceneSet1Layout.get(), resources);
    }

    void EngineSceneBindingSets::UpdatePerObjectModel(RHIBuffer* perObjectBuffer, const Matrix4& model) const
    {
        if (perObjectBuffer)
        {
            perObjectBuffer->UpdateSubresource(&model, 0, sizeof(Matrix4));
        }
    }
}
