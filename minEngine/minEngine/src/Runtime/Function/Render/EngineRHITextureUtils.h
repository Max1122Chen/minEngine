#pragma once

#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHITexture.h"

namespace minEngine
{
    inline RHITextureCreateDesc MakeDepthTextureDesc(
        uint32_t width,
        uint32_t height,
        RHITextureDimension dimension = RHITextureDimension::Texture2D,
        uint32_t layers = 1)
    {
        RHITextureCreateDesc desc;
        desc.Dimension = dimension;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArrayLayers = layers;
        desc.Format = TextureFormat::DEPTH32;
        desc.Flags = RHITextureCreateFlags::RenderTarget | RHITextureCreateFlags::ShaderResource;
        return desc;
    }

    inline RHIBufferCreateDesc MakeUniformBufferDesc(uint32_t byteSize)
    {
        RHIBufferCreateDesc desc;
        desc.Usage = RHIBufferUsage::Uniform;
        desc.ByteSize = byteSize;
        return desc;
    }

    inline RHITextureCreateDesc MakeTexture2DBindingDesc(uint32_t width, uint32_t height, TextureFormat format)
    {
        RHITextureCreateDesc desc;
        desc.Dimension = RHITextureDimension::Texture2D;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArrayLayers = 1;
        desc.Format = format;
        desc.Flags = RHITextureCreateFlags::ShaderResource;
        return desc;
    }

    inline RHITextureCreateDesc MakeTextureCubeBindingDesc(uint32_t size, TextureFormat format, uint32_t numMips = 1)
    {
        RHITextureCreateDesc desc;
        desc.Dimension = RHITextureDimension::TextureCube;
        desc.Width = size;
        desc.Height = size;
        desc.DepthOrArrayLayers = 6;
        desc.Format = format;
        desc.Flags = RHITextureCreateFlags::ShaderResource;
        desc.NumMips = numMips;
        return desc;
    }

    inline RHITextureCreateDesc MakeTextureCubeRenderTargetDesc(
        uint32_t size,
        TextureFormat format,
        uint32_t numMips = 1)
    {
        RHITextureCreateDesc desc = MakeTextureCubeBindingDesc(size, format, numMips);
        desc.Flags = RHITextureCreateFlags::RenderTarget | RHITextureCreateFlags::ShaderResource;
        return desc;
    }
}
