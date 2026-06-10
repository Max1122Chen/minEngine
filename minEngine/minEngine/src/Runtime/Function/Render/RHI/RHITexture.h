#pragma once
#include "Core.h"

#include <cstdint>
#include <memory>

namespace minEngine
{
    enum class TextureFormat
    {
        None = 0,
        RED,
        RGB8,
        RGBA8,
        RGB16F,
        RGBA16F,
        DEPTH16,
        DEPTH24,
        DEPTH32,
        DEPTH24STENCIL8
    };

    enum class TextureUsage
    {
        None = 0,
        TextureBinding,
        Color,
        Depth,
        Stencil,
        DepthStencil,
    };

    // --- Modern RHI (S2) ---

    enum class RHITextureDimension : uint8_t
    {
        Texture2D,
        TextureCube,
        Texture2DArray,
    };

    enum class RHITextureCreateFlags : uint32_t
    {
        None = 0,
        RenderTarget = 1u << 0,
        ShaderResource = 1u << 1,
        GenerateMips = 1u << 2,
    };

    inline RHITextureCreateFlags operator|(RHITextureCreateFlags a, RHITextureCreateFlags b)
    {
        return static_cast<RHITextureCreateFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline RHITextureCreateFlags operator&(RHITextureCreateFlags a, RHITextureCreateFlags b)
    {
        return static_cast<RHITextureCreateFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    struct RHITextureCreateDesc
    {
        RHITextureDimension Dimension = RHITextureDimension::Texture2D;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t DepthOrArrayLayers = 1;
        TextureFormat Format = TextureFormat::None;
        RHITextureCreateFlags Flags = RHITextureCreateFlags::None;
        uint32_t NumMips = 1;
    };

    class RHITexture
    {
    public:
        virtual ~RHITexture() = default;

        virtual const RHITextureCreateDesc& GetDesc() const = 0;

        virtual void* GetNativeResource() const { return nullptr; }

        // OpenGL: GLuint texture name. Legacy RHITexture2D uses GetID() until S5+ removal.
        virtual uint32_t GetNativeHandle() const
        {
            return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(GetNativeResource()));
        }
    };

    inline uint32_t GetRHINativeTextureHandle(const RHITexture* texture)
    {
        return texture ? texture->GetNativeHandle() : 0;
    }

    using RHITextureRef = std::shared_ptr<RHITexture>;
}
