#pragma once

#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHISampler.h"
#include "Render/RHI/RHIShaderBinding.h"
#include "Render/RHI/RHITexture.h"
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
        static constexpr uint32_t kBonePaletteMatrices = 256;

        void Initialize(RHICommandList& cmdList);
        void Shutdown();

        void BeginFrame(
            RHICommandList& cmdList,
            RHIBuffer* perFrame,
            RHIBuffer* lights,
            RHIBuffer* perObjectRing,
            uint32_t perObjectSlotStride,
            RHIBuffer* bonePaletteRing = nullptr,
            uint32_t bonePaletteSlotStride = 0);

        /**
         * Route the per-frame block that per-object set0 views while a screen-space pass records.
         * The block is read when a draw is replayed, so a role that publishes different matrices
         * needs a block of its own instead of overwriting the camera block the 3D draws point at.
         * Passing nullptr returns set0 to the scene camera block.
         */
        void SetScreenSpacePerFrame(RHIBuffer* block)
        {
            if (block != nullptr && block != m_ScreenSpaceSet0Block)
            {
                m_ScreenSpaceSet0Block = block;
                m_ScreenSpaceSet0BySlot.clear();
            }

            m_ScreenSpacePerFrame = block;
        }

        /**
         * Write next ring slot and return a set0 that views that slot (scene opaque/translucent).
         *
         * The reference is returned instead of the address because the caller keeps it: a packet
         * records the set it names, and a stream that has not been replayed yet names it too, so
         * this object clearing its cache must not be what ends the set's life.
         */
        RHIShaderBindingSetRef BindNextPerObjectModel(
            RHICommandList& cmdList,
            const Matrix4& model,
            const Matrix4* bonePalette = nullptr,
            uint32_t boneCount = 0);

        /**
         * Write next ring slot for ShadowPass (which uses a different set layout).
         * Returns byte offset into the per-object ring; caller builds its own binding set.
         */
        uint32_t WriteNextPerObjectModel(RHICommandList& cmdList, const Matrix4& model);

        void BuildSceneSet1(
            RHICommandList& cmdList,
            const SceneRenderContext& ctx,
            RHIBuffer* dirLightViewProjs,
            RHIBuffer* cascadeFarPlanes,
            RHIBuffer* spotLightViewProjs);

        /** Drop cached shadow SRVs / set1 after RDG shadow resources are recreated or removed. */
        void InvalidateShadowTextureBindings();

        /** Borrowed access for identity checks; a caller that keeps it takes a copy. */
        const RHIShaderBindingSetRef& GetSceneSet1() const { return m_SceneSet1; }
        RHIShaderBindingSetLayout* GetSceneSet0Layout() const { return m_SceneSet0Layout.get(); }
        RHIShaderBindingSetLayout* GetSceneSet1Layout() const { return m_SceneSet1Layout.get(); }

        RHIBuffer* GetPerObjectRingBuffer() const { return m_PerObjectRing; }
        uint32_t GetPerObjectSlotStride() const { return m_PerObjectSlotStride; }
        RHIBuffer* GetBonePaletteRingBuffer() const { return m_BonePaletteRing; }
        uint32_t GetBonePaletteSlotStride() const { return m_BonePaletteSlotStride; }

    private:
        struct ShadowTextureSlotCache
        {
            RHITexture* Texture = nullptr;
            uint32_t PhysicalIndex = UINT32_MAX;
            RHITextureCreateDesc Desc{};
        };

        bool ShadowTextureSlotChanged(
            RHITexture* texture,
            uint32_t physicalIndex,
            ShadowTextureSlotCache& cache);

        RHIShaderBindingSetLayoutRef m_SceneSet0Layout;
        RHIShaderBindingSetLayoutRef m_SceneSet1Layout;
        RHIShaderBindingSetRef m_SceneSet1;
        std::vector<RHIShaderBindingSetRef> m_SceneSet0BySlot;
        /** Same ring slots as m_SceneSet0BySlot, but viewing the screen-space per-frame block. */
        std::vector<RHIShaderBindingSetRef> m_ScreenSpaceSet0BySlot;
        RHIBuffer* m_ScreenSpacePerFrame = nullptr;
        RHIBuffer* m_ScreenSpaceSet0Block = nullptr;

        RHIShaderResourceViewRef GetOrCreateTextureSRV(RHICommandList& cmdList, RHITexture* texture, int32_t arraySlice = -1);

        RHITextureViewCache m_TextureViewCache;
        std::shared_ptr<RHIShaderResourceView> m_DirShadowSRV;
        std::array<std::shared_ptr<RHIShaderResourceView>, MAX_SPOT_SHADOW_MAPS> m_SpotShadowSRVs{};
        std::array<std::shared_ptr<RHIShaderResourceView>, MAX_POINT_SHADOW_MAPS> m_PointShadowSRVs{};
        std::array<std::shared_ptr<RHIShaderResourceView>, 3> m_IblSRVs{};

        /** Pinned set1 samplers. */
        RHISamplerRef m_ShadowCompareSampler;
        RHISamplerRef m_IblLinearClampSampler;

        RHIBuffer* m_CachedPerFrame = nullptr;
        RHIBuffer* m_CachedLights = nullptr;
        RHIBuffer* m_PerObjectRing = nullptr;
        uint32_t m_PerObjectSlotStride = 256;
        uint32_t m_PerObjectWriteIndex = 0;

        RHIBuffer* m_BonePaletteRing = nullptr;
        uint32_t m_BonePaletteSlotStride = 0;
        RHIBufferRef m_IdentityBonePalette;

        ShadowTextureSlotCache m_CachedDirShadowSlot{};
        std::array<ShadowTextureSlotCache, MAX_SPOT_SHADOW_MAPS> m_CachedSpotShadowSlots{};
        std::array<ShadowTextureSlotCache, MAX_POINT_SHADOW_MAPS> m_CachedPointShadowSlots{};
        RHIBuffer* m_CachedDirLightViewProjs = nullptr;
        RHIBuffer* m_CachedCascadeFarPlanes = nullptr;
        RHIBuffer* m_CachedSpotLightViewProjs = nullptr;
        RHITexture* m_CachedIblIrradianceTexture = nullptr;
        RHITexture* m_CachedIblPrefilterTexture = nullptr;
        RHITexture* m_CachedIblBrdfLutTexture = nullptr;

        uint32_t m_ShadowBindingGeneration = 0;
        uint32_t m_BuiltShadowBindingGeneration = UINT32_MAX;
    };
}
