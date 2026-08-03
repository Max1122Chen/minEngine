#pragma once

#include "Core.h"
#include "ShadowTypes.h"

#include <unordered_map>

namespace minEngine
{
    class SpotLightSceneProxy;
    class PointLightSceneProxy;

    /**
     * Shadow slot metadata (texture units / resolution bookkeeping).
     * RND-F08: textures are owned by RenderGraph SetupAttachments — this type must not Create.
     */
    class ShadowResourceManager
    {
    public:
        ShadowResourceManager() = default;
        ~ShadowResourceManager() = default;

        void Initialize();
        void Shutdown();

        void BeginFrame(uint64_t frameIndex);
        void EndFrame();

        ShadowResourceHandle AcquireDirectional(const ShadowRequest& req, uint32_t cascadeCount);
        ShadowResourceHandle AcquireSpot(const ShadowRequest& req);
        ShadowResourceHandle AcquirePoint(const ShadowRequest& req);

    private:
        struct SpotShadowSlot
        {
            ShadowResolution Resolution{};
            int TextureUnit = -1;
        };

        struct PointShadowSlot
        {
            ShadowResolution Resolution{};
            int TextureUnit = -1;
        };

        uint64_t m_FrameIndex = 0;
        int m_DirectionalTextureUnit = 8;

        std::unordered_map<SpotLightSceneProxy*, SpotShadowSlot> m_SpotShadowSlots;
        std::unordered_map<PointLightSceneProxy*, PointShadowSlot> m_PointShadowSlots;
    };
}
