#include "Texture.h"
#include "RenderSystem.h"
#include "RHI/RHI.h"
#include "RHI/RHITexture.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    std::shared_ptr<Texture2D> Texture2D::CreateSolidRGBA(RHI& rhi, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        const unsigned char pixel[] = { r, g, b, a };
        std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>();
        texture->m_Width = 1;
        texture->m_Height = 1;
        texture->m_Channels = 4;
        texture->m_RHITexture = rhi.CreateRHITexture2D(pixel, RHITextureDesc{
            .Width = 1,
            .Height = 1,
            .Format = TextureFormat::RGBA8,
            .Usage = TextureUsage::TextureBinding,
        });
        return texture;
    }
}