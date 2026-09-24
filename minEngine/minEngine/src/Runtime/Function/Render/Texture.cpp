#include "Texture.h"
#include "EngineRHITextureUtils.h"
#include "RHI/RHI.h"
#include "RHI/RHITexture.h"

namespace minEngine
{
    namespace
    {
        TextureFiltering ToTextureFiltering(RHISamplerFilter filter)
        {
            return filter == RHISamplerFilter::Nearest ? TextureFiltering::Nearest : TextureFiltering::Linear;
        }

        TextureWrapping ToTextureWrapping(RHISamplerAddress address)
        {
            switch (address)
            {
            case RHISamplerAddress::MirroredRepeat:
                return TextureWrapping::MirroredRepeat;
            case RHISamplerAddress::ClampToEdge:
                return TextureWrapping::ClampToEdge;
            case RHISamplerAddress::ClampToBorder:
                return TextureWrapping::ClampToBorder;
            case RHISamplerAddress::Repeat:
            default:
                return TextureWrapping::Repeat;
            }
        }
    }

    void Texture2D::SetPreferredSamplerDesc(const RHISamplerDesc& desc)
    {
        m_PreferredSamplerDesc = desc;
        m_Filtering = ToTextureFiltering(desc.MagFilter);
        m_Wrapping = ToTextureWrapping(desc.AddressU);
    }

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

    std::shared_ptr<Texture2D> Texture2D::CreateFromRGBA8(RHI& rhi, uint32_t width, uint32_t height, const uint8_t* rgba)
    {
        if (width == 0 || height == 0 || rgba == nullptr)
        {
            return nullptr;
        }

        std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>();
        texture->m_Width = width;
        texture->m_Height = height;
        texture->m_Channels = 4;
        texture->m_RHITexture = rhi.RHICreateTexture2D(
            MakeTexture2DBindingDesc(width, height, TextureFormat::RGBA8),
            rgba);
        return texture;
    }
}
