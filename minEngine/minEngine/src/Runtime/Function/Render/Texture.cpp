#include "Texture.h"
#include "EngineRHITextureUtils.h"
#include "RHI/RHI.h"
#include "RHI/RHITexture.h"

namespace minEngine
{
    std::shared_ptr<Texture2D> Texture2D::CreateSolidRGBA(RHI& rhi, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        const unsigned char pixel[] = { r, g, b, a };
        std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>();
        texture->m_Width = 1;
        texture->m_Height = 1;
        texture->m_Channels = 4;
        texture->m_RHITexture = rhi.RHICreateTexture2D(
            MakeTexture2DBindingDesc(1, 1, TextureFormat::RGBA8),
            pixel);
        return texture;
    }
}