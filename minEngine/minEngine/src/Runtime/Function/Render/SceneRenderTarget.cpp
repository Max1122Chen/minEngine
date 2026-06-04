#include "SceneRenderTarget.h"

#include "Runtime/Function/Render/OpenGL/OpenGLRHIModern.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHITexture.h"
#include "Render/RHI/RHIBuffers.h"

namespace minEngine
{
    void SceneRenderTarget::RefreshModernTextureWrappers()
    {
        m_ColorTextureRHI = OpenGLRHITexture::WrapLegacy2D(m_ColorTexture);
        m_DepthTextureRHI = OpenGLRHITexture::WrapLegacy2D(m_DepthTexture);
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

        RefreshModernTextureWrappers();
    }

    RHIRenderPassInfo SceneRenderTarget::BuildRenderPassInfo() const
    {
        RHIRenderPassInfo info(
            m_ColorTextureRHI.get(),
            RHIRenderTargetActions::ClearStore,
            m_DepthTextureRHI.get(),
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
        m_ColorTextureRHI.reset();
        m_DepthTextureRHI.reset();
        m_ColorTexture.reset();
        m_DepthTexture.reset();
        m_FrameBuffer.reset();
        m_Width = 0;
        m_Height = 0;
    }
}
