#include "SceneRenderTarget.h"

#include "Render/RHI/RHI.h"
#include "Render/RHI/RHITexture.h"
#include "Render/RHI/RHIBuffers.h"

namespace minEngine
{
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

        if (m_ColorTexture && m_ColorTexture->GetWidth() == width && m_ColorTexture->GetHeight() == height)
        {
            return;
        }

        m_FrameBuffer = rhi->CreateFrameBuffer(width, height);
        m_Width = width;
        m_Height = height;

        RHITextureDesc colorDesc{
            .Width = width,
            .Height = height,
            .Format = TextureFormat::RGBA8,
            .Usage = TextureUsage::Color
        };

        RHITextureDesc depthDesc{
            .Width = width,
            .Height = height,
            .Format = TextureFormat::DEPTH24STENCIL8,
            .Usage = TextureUsage::DepthStencil
        };

        m_ColorTexture = rhi->CreateRHITexture2D(nullptr, colorDesc);
        m_DepthTexture = rhi->CreateRHITexture2D(nullptr, depthDesc);

        m_FrameBuffer->AttachColorBuffer(m_ColorTexture);
        m_FrameBuffer->AttachDepthStencilBuffer(m_DepthTexture);
    }

    void SceneRenderTarget::Shutdown()
    {
        m_ColorTexture.reset();
        m_DepthTexture.reset();
        m_FrameBuffer.reset();
        m_Width = 0;
        m_Height = 0;
    }
}
