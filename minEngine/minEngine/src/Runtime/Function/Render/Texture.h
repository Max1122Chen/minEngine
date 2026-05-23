#pragma once
#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Resource/Asset.h"

namespace minEngine
{
    class AssetManager;
    class RHI;
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

    ME_CLASS()
    class Texture2D : public Asset
    {
        ME_GENERATED_BODY(Texture2D)
    public:
        Texture2D() = default;

        virtual ~Texture2D() = default;

        RHITexture2D* GetRHITexture() const { return m_RHITexture.get(); }
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        TextureWrapping GetWrapping() const { return m_Wrapping; }
        TextureFiltering GetFiltering() const { return m_Filtering; }
        uint32_t GetChannels() const { return m_Channels; }

        static std::shared_ptr<Texture2D> CreateSolidRGBA(RHI& rhi, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

    protected:
        friend class Texture2DLoader;

        std::shared_ptr<RHITexture2D> m_RHITexture;
        uint32_t m_Width {0};
        uint32_t m_Height {0};
        uint32_t m_Channels {0};
        TextureWrapping m_Wrapping {TextureWrapping::Repeat};
        TextureFiltering m_Filtering {TextureFiltering::Linear};
    };

    class TextureCubeLoader;

    class TextureCube
    {
    public:
        TextureCube() = default;

        virtual ~TextureCube() = default;

        RHITextureCube* GetRHITexture() const { return m_RHITexture.get(); }
        uint32_t GetSize() const { return m_Size; }
        TextureWrapping GetWrapping() const { return m_Wrapping; }
        TextureFiltering GetFiltering() const { return m_Filtering; }

    protected:
        friend class TextureCubeLoader;

        std::shared_ptr<RHITextureCube> m_RHITexture;
        uint32_t m_Size; // Cube maps are square, so we can just store one dimension
        uint32_t m_Channels;
        TextureWrapping m_Wrapping;
        TextureFiltering m_Filtering;
    };

}

#include "Texture.gen.h"