#pragma once

#include "Render/RHI/RHIShaderBinding.h"
#include "Render/RHI/RHITextureViewCache.h"
#include "Runtime/Function/Render/SceneRenderContext.h"

#include <cstdint>
#include <vector>

namespace minEngine
{
    class RHICommandList;
    class RHIBuffer;
    class RHITexture;

    /**
     * Scene descriptor sets (set0 / set1) plus a Per-Object UBO ring.
     * Vulkan records many draws per CB; a single host-visible u_Model slot would be
     * overwritten before submit — each draw must bind a distinct aligned region.
     */
    class EngineSceneBindingSets
    {
    public:
        static constexpr uint32_t kPerObjectRingSlots = 512;

        void Initialize(RHICommandList& cmdList);
        void Shutdown();

        void BeginFrame(
            RHICommandList& cmdList,
            RHIBuffer* perFrame,
            RHIBuffer* lights,
            RHIBuffer* perObjectRing,
            uint32_t perObjectSlotStride);

        /** Write next ring slot and return a set0 that views that slot (scene opaque/translucent). */
        RHIShaderBindingSet* BindNextPerObjectModel(RHICommandList& cmdList, const Matrix4& model);

        /**
         * Write next ring slot for ShadowPass (which uses a different set layout).
         * Returns byte offset into the per-object ring; caller builds its own binding set.
         */
        uint32_t WriteNextPerObjectModel(const Matrix4& model);

        void BuildSceneSet1(
            RHICommandList& cmdList,
            const SceneRenderContext& ctx,
            RHIBuffer* dirLightViewProjs,
            RHIBuffer* cascadeFarPlanes,
            RHIBuffer* spotLightViewProjs);

        RHIShaderBindingSet* GetSceneSet1() const { return m_SceneSet1.get(); }
        RHIShaderBindingSetLayout* GetSceneSet0Layout() const { return m_SceneSet0Layout.get(); }
        RHIShaderBindingSetLayout* GetSceneSet1Layout() const { return m_SceneSet1Layout.get(); }

        RHIBuffer* GetPerObjectRingBuffer() const { return m_PerObjectRing; }
        uint32_t GetPerObjectSlotStride() const { return m_PerObjectSlotStride; }

    private:
        RHIShaderBindingSetLayoutRef m_SceneSet0Layout;
        RHIShaderBindingSetLayoutRef m_SceneSet1Layout;
        RHIShaderBindingSetRef m_SceneSet1;
        std::vector<RHIShaderBindingSetRef> m_SceneSet0BySlot;

        RHIShaderResourceViewRef GetOrCreateTextureSRV(RHICommandList& cmdList, RHITexture* texture, int32_t arraySlice = -1);

        RHITextureViewCache m_TextureViewCache;
        std::shared_ptr<RHIShaderResourceView> m_DirShadowSRV;
        std::array<std::shared_ptr<RHIShaderResourceView>, MAX_SPOT_SHADOW_MAPS> m_SpotShadowSRVs{};
        std::array<std::shared_ptr<RHIShaderResourceView>, MAX_POINT_SHADOW_MAPS> m_PointShadowSRVs{};
        std::array<std::shared_ptr<RHIShaderResourceView>, 3> m_IblSRVs{};

        RHIBuffer* m_CachedPerFrame = nullptr;
        RHIBuffer* m_CachedLights = nullptr;
        RHIBuffer* m_PerObjectRing = nullptr;
        uint32_t m_PerObjectSlotStride = 256;
        uint32_t m_PerObjectWriteIndex = 0;

        RHITexture* m_CachedDirShadowTexture = nullptr;
        std::array<RHITexture*, MAX_SPOT_SHADOW_MAPS> m_CachedSpotShadowTextures{};
        std::array<RHITexture*, MAX_POINT_SHADOW_MAPS> m_CachedPointShadowTextures{};
        RHIBuffer* m_CachedDirLightViewProjs = nullptr;
        RHIBuffer* m_CachedCascadeFarPlanes = nullptr;
        RHIBuffer* m_CachedSpotLightViewProjs = nullptr;
        RHITexture* m_CachedIblIrradianceTexture = nullptr;
        RHITexture* m_CachedIblPrefilterTexture = nullptr;
        RHITexture* m_CachedIblBrdfLutTexture = nullptr;
    };
}
