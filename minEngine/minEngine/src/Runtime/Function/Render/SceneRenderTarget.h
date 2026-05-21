#pragma once

#include "Core.h"

#include <memory>

namespace minEngine
{
    class FrameBuffer;
    class RHITexture2D;
    class RHI;

    /** Owned by SceneViewport (P2+). P0/P1: resources still live on RenderPipeline. */
    class SceneRenderTarget
    {
    public:
        void Initialize(RHI* rhi, uint32_t width, uint32_t height);
        void Resize(RHI* rhi, uint32_t width, uint32_t height);
        void Shutdown();

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        const std::shared_ptr<RHITexture2D>& GetColorTexture() const { return m_ColorTexture; }
        FrameBuffer* GetFrameBuffer() const { return m_FrameBuffer.get(); }

    private:
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        std::shared_ptr<FrameBuffer> m_FrameBuffer;
        std::shared_ptr<RHITexture2D> m_ColorTexture;
        std::shared_ptr<RHITexture2D> m_DepthTexture;
    };
}
