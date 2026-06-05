#pragma once

#include "Core.h"
#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHITexture.h"

#include <memory>

namespace minEngine
{
    class RHI;

    /** Owned by SceneViewport (P2+). GPU color/depth via modern RHI only (RND-F03-S1). */
    class SceneRenderTarget
    {
    public:
        void Initialize(RHI* rhi, uint32_t width, uint32_t height);
        void Resize(RHI* rhi, uint32_t width, uint32_t height);
        void Shutdown();

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        const RHITextureRef& GetColorTexture() const { return m_ColorTexture; }
        const RHITextureRef& GetDepthTexture() const { return m_DepthTexture; }

        RHIRenderPassInfo BuildRenderPassInfo() const;

    private:
        static RHITextureCreateDesc MakeColorDesc(uint32_t width, uint32_t height);
        static RHITextureCreateDesc MakeDepthDesc(uint32_t width, uint32_t height);

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        RHITextureRef m_ColorTexture;
        RHITextureRef m_DepthTexture;
    };
}
