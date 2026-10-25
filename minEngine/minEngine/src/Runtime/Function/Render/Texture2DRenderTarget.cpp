#include "Texture2DRenderTarget.h"

#include "EngineRHITextureUtils.h"
#include "RHI/RHI.h"
#include "Runtime/Core/Object/ObjectManager.h"

namespace minEngine
{
    std::shared_ptr<Texture2DRenderTarget> Texture2DRenderTarget::Create(
        RHI& rhi,
        uint32_t width,
        uint32_t height,
        TextureFormat format)
    {
        if (width == 0 || height == 0 || format == TextureFormat::None)
        {
            return nullptr;
        }

        std::shared_ptr<Texture2DRenderTarget> texture = NewObject<Texture2DRenderTarget>("Texture2DRenderTarget");
        if (!texture->AllocateGpu(rhi, width, height, format))
        {
            return nullptr;
        }
        return texture;
    }

    bool Texture2DRenderTarget::Resize(RHI& rhi, uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            return false;
        }
        return AllocateGpu(rhi, width, height, m_RenderTargetFormat);
    }

    bool Texture2DRenderTarget::AllocateGpu(RHI& rhi, uint32_t width, uint32_t height, TextureFormat format)
    {
        RHITextureRef gpu = rhi.RHICreateTexture2D(MakeTexture2DRenderTargetDesc(width, height, format), nullptr);
        if (!gpu)
        {
            return false;
        }

        m_RHITexture = std::move(gpu);
        m_Width = width;
        m_Height = height;
        m_Channels = 4;
        m_RenderTargetFormat = format;
        RebuildPreferredSamplerDesc();
        return true;
    }
}
