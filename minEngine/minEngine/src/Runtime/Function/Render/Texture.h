#pragma once
#include "Core.h"

namespace minEngine
{
    class AssetManager;
    class RHITexture2D;
    class RHITextureCube;

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

        virtual ~Texture2D() = default;

        RHITexture2D* GetRHITexture() const { return m_RHITexture.get(); }
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        TextureWrapping GetWrapping() const { return m_Wrapping; }
        TextureFiltering GetFiltering() const { return m_Filtering; }
        uint32_t GetChannels() const { return m_Channels; }

    protected:
        friend class AssetManager;

        std::shared_ptr<RHITexture2D> m_RHITexture;
        uint32_t m_Width {0};
        uint32_t m_Height {0};
        uint32_t m_Channels {0};
        TextureWrapping m_Wrapping {TextureWrapping::Repeat};
        TextureFiltering m_Filtering {TextureFiltering::Linear};
    };

    class TextureCube
    {
    public:
        TextureCube() = default;
        TextureCube(const std::vector<std::string>& facePaths, uint32_t unit,
                    TextureWrapping wrapping = TextureWrapping::ClampToEdge,
                    TextureFiltering filtering = TextureFiltering::Linear);

        virtual ~TextureCube() = default;

        RHITextureCube* GetRHITexture() const { return m_RHITexture.get(); }
        uint32_t GetSize() const { return m_Size; }
        TextureWrapping GetWrapping() const { return m_Wrapping; }
        TextureFiltering GetFiltering() const { return m_Filtering; }

    protected:
        std::shared_ptr<RHITextureCube> m_RHITexture;
        uint32_t m_Size; // Cube maps are square, so we can just store one dimension
        uint32_t m_Channels;
        TextureWrapping m_Wrapping;
        TextureFiltering m_Filtering;
    };

}