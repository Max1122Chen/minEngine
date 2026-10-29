#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RHI/RHIDeferredCreate.h"

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

    inline bool HasTextureCreateFlag(RHITextureCreateFlags flags, RHITextureCreateFlags bit)
    {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(bit)) != 0u;
    }

    /** Full mip chain count for a square (or max-edge) texture size. */
    inline uint32_t ComputeTextureMipCount(uint32_t width, uint32_t height = 0)
    {
        const uint32_t size = height == 0 ? width : (width > height ? width : height);
        if (size == 0)
        {
            return 1;
        }

        uint32_t mipCount = 1;
        uint32_t dimension = size;
        while (dimension > 1)
        {
            dimension >>= 1;
            ++mipCount;
        }
        return mipCount;
    }

    /** Bytes one texel of this format occupies in CPU memory; 0 for TextureFormat::None. */
    inline uint32_t GetTextureFormatByteSize(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::RED:
            return 1;
        case TextureFormat::RGB8:
            return 3;
        case TextureFormat::RGBA8:
            return 4;
        case TextureFormat::RGB16F:
            return 6;
        case TextureFormat::RGBA16F:
            return 8;
        case TextureFormat::DEPTH16:
            return 2;
        case TextureFormat::DEPTH24:
            return 3;
        case TextureFormat::DEPTH32:
        case TextureFormat::DEPTH24STENCIL8:
            return 4;
        default:
            return 0;
        }
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

    /**
     * A texture handle. Like a buffer, the native object behind it may be created after this call
     * returns: a texture asked for off the graphics role gets its description and a CPU copy of
     * the initial pixels now, and the driver object at the start of the next render frame.
     */
    class RHITexture : public RHIDeferredCreateTarget
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
