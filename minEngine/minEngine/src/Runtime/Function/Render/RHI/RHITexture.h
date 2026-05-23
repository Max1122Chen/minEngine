#pragma once
#include "Core.h"

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

    struct RHITextureDesc
    {
        uint32_t        Width       = 0;
        uint32_t        Height      = 0;
        uint32_t        Layers      = 1;
        TextureFormat   Format      = TextureFormat::None;
        TextureUsage    Usage       = TextureUsage::None;
    };

    class RHITexture2D
    {
    public:
        RHITexture2D() = default;
        virtual ~RHITexture2D() = default;

        uint32_t GetID() const { return m_ID; }
        int GetUnit() const { return m_Unit; }
        const RHITextureDesc& GetDesc() const { return m_Desc; }
        const uint32_t GetWidth() const { return m_Desc.Width; }
        const uint32_t GetHeight() const { return m_Desc.Height; }
        const TextureFormat GetFormat() const { return m_Desc.Format; }
        const TextureUsage GetUsage() const { return m_Desc.Usage; }

        virtual void Bind(int unit) = 0;
        virtual void Unbind() = 0;

    protected:
        uint32_t m_ID { 0 };
        int m_Unit { 0 };
        RHITextureDesc m_Desc;

    };

    class RHITextureCube
    {
    public:
        RHITextureCube() = default;
        virtual ~RHITextureCube() = default;

        uint32_t GetID() const { return m_ID; }
        int GetUnit() const { return m_Unit; }
        const RHITextureDesc& GetDesc() const { return m_Desc; }
        const uint32_t GetWidth() const { return m_Desc.Width; }
        const uint32_t GetHeight() const { return m_Desc.Height; }
        const TextureFormat GetFormat() const { return m_Desc.Format; }
        const TextureUsage GetUsage() const { return m_Desc.Usage; }

        virtual void Bind(int unit) = 0;
        virtual void Unbind() = 0;

    protected:
        uint32_t m_ID { 0 };
        int m_Unit { 0 };
        RHITextureDesc m_Desc;
    };

    class RHITexture2DArray
    {
    public:
        RHITexture2DArray() = default;
        virtual ~RHITexture2DArray() = default;

        uint32_t GetID() const { return m_ID; }
        int GetUnit() const { return m_Unit; }
        const RHITextureDesc& GetDesc() const { return m_Desc; }
        const uint32_t GetWidth() const { return m_Desc.Width; }
        const uint32_t GetHeight() const { return m_Desc.Height; }
        const uint32_t GetLayers() const { return m_Desc.Layers; }
        const TextureFormat GetFormat() const { return m_Desc.Format; }
        const TextureUsage GetUsage() const { return m_Desc.Usage; }

        virtual void Bind(int unit) = 0;
        virtual void Unbind() = 0;

    protected:
        uint32_t m_ID { 0 };
        int m_Unit { 0 };
        RHITextureDesc m_Desc;
    };

}