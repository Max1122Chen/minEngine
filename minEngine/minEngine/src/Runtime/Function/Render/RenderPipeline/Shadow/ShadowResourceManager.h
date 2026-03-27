#pragma once

#include "Core.h"
#include "ShadowTypes.h"

namespace minEngine
{
    class RHI;
    class RHITexture2DArray;
    class RHITextureCube;

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

        std::shared_ptr<RHITexture2DArray> GetDirectionalShadowArray() const { return m_DirectionalShadowArray; }

    private:
        struct DirectionalArrayResource
        {
            ShadowResolution Resolution{};
            uint32_t Layers = 0;
            int TextureUnit = 8;
        };

    private:
        ShadowResourceHandle BuildInvalidHandle() const;
        bool EnsureDirectionalResource(const ShadowRequest& req, uint32_t cascadeCount);

    private:
        RHI* m_RHI = nullptr; // Non-owning pointer.
        uint64_t m_FrameIndex = 0;

        DirectionalArrayResource m_DirectionalConfig{};
        std::shared_ptr<RHITexture2DArray> m_DirectionalShadowArray;
    };
}
