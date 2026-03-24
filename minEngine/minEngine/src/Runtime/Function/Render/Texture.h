#pragma once
#include "Core.h"

namespace minEngine
{
    class RHITexture2D;

    enum class TextureWrapping
    {
        None = 0,
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder
    };

    enum class TextureFiltering
    {
        None = 0,
        Nearest,
        Linear,
        NearestMipmapNearest,
        LinearMipmapNearest,
        NearestMipmapLinear,
        LinearMipmapLinear
    };

    class Texture2D
    {
    public:
        Texture2D() = default;
        Texture2D(const std::string& path, uint32_t unit,
                  TextureWrapping wrapping = TextureWrapping::Repeat,
                  TextureFiltering filtering = TextureFiltering::Linear);

        virtual ~Texture2D() = default;

        RHITexture2D* GetRHITexture() const { return m_RHITexture.get(); }
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        TextureWrapping GetWrapping() const { return m_Wrapping; }
        TextureFiltering GetFiltering() const { return m_Filtering; }
        uint32_t GetChannels() const { return m_Channels; }

    protected:
        std::shared_ptr<RHITexture2D> m_RHITexture;
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_Channels;
        TextureWrapping m_Wrapping;
        TextureFiltering m_Filtering;
    };
}