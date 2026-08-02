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
        // RND-F07: size bookkeeping only; graph owns/creates frame RTs via SetupAttachments.
        if (!rhi || width == 0 || height == 0)
        {
            return;
        }

        if (m_Width == width && m_Height == height)
        {
            return;
        }

        m_Width = width;
        m_Height = height;
        // Drop published views so UI does not keep a mismatched size; graph will republish.
        m_ColorTexture.reset();
        m_DepthTexture.reset();
    }

    void SceneRenderTarget::PublishGraphColorTexture(RHITextureRef colorTexture)
    {
        m_ColorTexture = std::move(colorTexture);
        if (m_ColorTexture)
        {
            m_Width = m_ColorTexture->GetDesc().Width;
            m_Height = m_ColorTexture->GetDesc().Height;
        }
    }

    void SceneRenderTarget::PublishGraphDepthTexture(RHITextureRef depthTexture)
    {
        m_DepthTexture = std::move(depthTexture);
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
