#pragma once

#include "Core.h"
#include "ShadowTypes.h"

#include <unordered_map>

namespace minEngine
{
    class RHI;
    class SpotLightSceneProxy;
    class PointLightSceneProxy;

    class ShadowResourceManager
    {
    public:
        ShadowResourceManager() = default;
        ~ShadowResourceManager() = default;

        void Initialize(RHI* rhi);
        void Shutdown();

        void BeginFrame(uint64_t frameIndex);
        void EndFrame();

        ShadowResourceHandle AcquireDirectional(const ShadowRequest& req, uint32_t cascadeCount);
        ShadowResourceHandle AcquireSpot(const ShadowRequest& req);
        ShadowResourceHandle AcquirePoint(const ShadowRequest& req);

        RHITextureRef GetDirectionalShadowArray() const { return m_DirectionalShadowArray; }

    private:
        struct DirectionalArrayResource
        {
            ShadowResolution Resolution{};
            uint32_t Layers = 0;
            int TextureUnit = 8;
        };

        struct SpotShadowResource
        {
            ShadowResolution Resolution{};
            RHITextureRef Texture;
            int TextureUnit = -1;
        };

        struct PointShadowResource
        {
            ShadowResolution Resolution{};
            RHITextureRef Texture;
            int TextureUnit = -1;
        };

    private:
        bool EnsureDirectionalResource(const ShadowRequest& req, uint32_t cascadeCount);
        bool EnsureSpotResource(const ShadowRequest& req, SpotShadowResource& resource);
        bool EnsurePointResource(const ShadowRequest& req, PointShadowResource& resource);

    private:
        RHI* m_RHI = nullptr;
        uint64_t m_FrameIndex = 0;

        DirectionalArrayResource m_DirectionalConfig{};
        RHITextureRef m_DirectionalShadowArray;

        std::unordered_map<SpotLightSceneProxy*, SpotShadowResource> m_SpotShadowResources;
        std::unordered_map<PointLightSceneProxy*, PointShadowResource> m_PointShadowResources;
    };
}
