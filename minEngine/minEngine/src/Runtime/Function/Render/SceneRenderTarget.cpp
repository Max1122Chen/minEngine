#include "SceneRenderTarget.h"

#include "Render/RHI/RHI.h"

namespace minEngine
{
    RHITextureCreateDesc SceneRenderTarget::MakeColorDesc(uint32_t width, uint32_t height)
    {
        RHITextureCreateDesc desc;
        desc.Dimension = RHITextureDimension::Texture2D;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArrayLayers = 1;
        desc.Format = TextureFormat::RGBA8;
        desc.Flags = RHITextureCreateFlags::RenderTarget | RHITextureCreateFlags::ShaderResource;
        desc.NumMips = 1;
        return desc;
    }

    RHITextureCreateDesc SceneRenderTarget::MakeDepthDesc(uint32_t width, uint32_t height)
    {
        RHITextureCreateDesc desc;
        desc.Dimension = RHITextureDimension::Texture2D;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArrayLayers = 1;
        desc.Format = TextureFormat::DEPTH24STENCIL8;
        desc.Flags = RHITextureCreateFlags::RenderTarget;
        desc.NumMips = 1;
        return desc;
    }

    void SceneRenderTarget::Initialize(RHI* rhi, uint32_t width, uint32_t height)
    {
        if (!rhi)
        {
            return;
        }
        Resize(rhi, width, height);
    }

    void SceneRenderTarget::Resize(RHI* rhi, uint32_t width, uint32_t height)
    {
        if (!rhi || width == 0 || height == 0)
        {
            return;
        }

        if (m_ColorTexture && m_ColorTexture->GetDesc().Width == width &&
            m_ColorTexture->GetDesc().Height == height)
        {
            return;
        }

        m_Width = width;
        m_Height = height;

        m_ColorTexture = rhi->RHICreateTexture2D(MakeColorDesc(width, height), nullptr);
        m_DepthTexture = rhi->RHICreateTexture2D(MakeDepthDesc(width, height), nullptr);
    }

    RHIRenderPassInfo SceneRenderTarget::BuildRenderPassInfo() const
    {
        RHIRenderPassInfo info(
            m_ColorTexture.get(),
            RHIRenderTargetActions::ClearStore,
            m_DepthTexture.get(),
            RHIDepthStencilTargetActions::ClearDepthStencilStoreDepthStencil);
        info.ClearValue.Color[0] = 0.1f;
        info.ClearValue.Color[1] = 0.1f;
        info.ClearValue.Color[2] = 0.1f;
        info.ClearValue.Color[3] = 1.0f;
        info.ClearValue.Depth = 1.0f;
        return info;
    }

    void SceneRenderTarget::Shutdown()
    {
        m_ColorTexture.reset();
        m_DepthTexture.reset();
        m_Width = 0;
        m_Height = 0;
    }
}
