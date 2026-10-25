#pragma once

#include "Texture.h"

namespace minEngine
{
    class RHI;

    /**
     * Sampleable Texture2D that owns a color render target (RenderTarget | ShaderResource).
     * Use with OffscreenRenderer::SetColorTarget for per-target bake without sharing RDG pool color.
     */
    ME_CLASS()
    class MINENGINE_API Texture2DRenderTarget : public Texture2D
    {
        ME_GENERATED_BODY()
    public:
        Texture2DRenderTarget() = default;
        ~Texture2DRenderTarget() override = default;

        static std::shared_ptr<Texture2DRenderTarget> Create(
            RHI& rhi,
            uint32_t width,
            uint32_t height,
            TextureFormat format = TextureFormat::RGBA8);

        bool Resize(RHI& rhi, uint32_t width, uint32_t height);

        TextureFormat GetRenderTargetFormat() const { return m_RenderTargetFormat; }

    protected:
        bool AllocateGpu(RHI& rhi, uint32_t width, uint32_t height, TextureFormat format);

        TextureFormat m_RenderTargetFormat = TextureFormat::RGBA8;
    };
}

#include "Texture2DRenderTarget.gen.h"
