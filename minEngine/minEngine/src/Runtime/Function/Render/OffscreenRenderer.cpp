#include "OffscreenRenderer.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/SceneViewport.h"
#include "Runtime/Function/Render/SceneRenderTarget.h"
#include "Runtime/Function/Render/Texture2DRenderTarget.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

namespace minEngine
{
    OffscreenRenderer::OffscreenRenderer() = default;

    OffscreenRenderer::OffscreenRenderer(OffscreenRenderer&& other) noexcept
        : m_Viewport(std::move(other.m_Viewport)),
          m_Width(other.m_Width),
          m_Height(other.m_Height),
          m_DrawDesc(other.m_DrawDesc),
          m_ColorTarget(std::move(other.m_ColorTarget)),
          m_Initialized(other.m_Initialized)
    {
        other.m_Viewport.reset();
        other.m_Initialized = false;
    }

    OffscreenRenderer& OffscreenRenderer::operator=(OffscreenRenderer&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }
        Shutdown();
        m_Viewport = std::move(other.m_Viewport);
        m_Width = other.m_Width;
        m_Height = other.m_Height;
        m_DrawDesc = other.m_DrawDesc;
        m_ColorTarget = std::move(other.m_ColorTarget);
        m_Initialized = other.m_Initialized;
        other.m_Viewport.reset();
        other.m_Initialized = false;
        return *this;
    }

    OffscreenRenderer::~OffscreenRenderer()
    {
        Shutdown();
    }

    void OffscreenRenderer::Initialize(uint32_t width, uint32_t height)
    {
        if (m_Initialized)
        {
            return;
        }

        RHI* rhi = ResolveRHI();
        if (rhi == nullptr || width == 0 || height == 0)
        {
            return;
        }

        m_Viewport = std::make_unique<SceneViewport>();
        m_Viewport->Initialize(rhi, width, height);
        m_Width = width;
        m_Height = height;

        // Default offscreen draw: no sky/post/shadows, no swapchain present.
        m_DrawDesc.Flags = SceneDrawFlags::None;
        // Default transparent clear (RGBA), typical for icon/thumbnail baking.
        m_DrawDesc.ClearColor[0] = 0.0f;
        m_DrawDesc.ClearColor[1] = 0.0f;
        m_DrawDesc.ClearColor[2] = 0.0f;
        m_DrawDesc.ClearColor[3] = 0.0f;
        m_DrawDesc.DepthClear = 1.0f;

        SyncViewportDesc();
        m_Initialized = true;
    }

    void OffscreenRenderer::Shutdown()
    {
        if (m_Viewport)
        {
            m_Viewport->Shutdown();
            m_Viewport.reset();
        }
        m_ColorTarget.reset();
        m_DrawDesc.ExternalSceneColor.reset();
        m_Width = 0;
        m_Height = 0;
        m_Initialized = false;
    }

    void OffscreenRenderer::SetColorTarget(const std::shared_ptr<Texture2DRenderTarget>& target)
    {
        m_ColorTarget = target;
    }

    void OffscreenRenderer::ClearColorTarget()
    {
        m_ColorTarget.reset();
        m_DrawDesc.ExternalSceneColor.reset();
    }

    void OffscreenRenderer::SyncViewportDesc()
    {
        if (!m_Viewport)
        {
            return;
        }
        const SceneDrawDesc vpDesc = m_Viewport->BuildDrawDesc(m_DrawDesc.Flags);
        m_DrawDesc.RenderTarget = vpDesc.RenderTarget;
        m_DrawDesc.ViewId = vpDesc.ViewId;
        if (m_DrawDesc.Camera == nullptr)
        {
            m_DrawDesc.Camera = vpDesc.Camera;
        }
    }

    RenderCamera* OffscreenRenderer::EnsureInternalCamera()
    {
        if (!m_Initialized || !m_Viewport)
        {
            return nullptr;
        }
        SyncViewportDesc();
        return m_DrawDesc.Camera;
    }

    void OffscreenRenderer::SetClearColor(float r, float g, float b, float a)
    {
        m_DrawDesc.ClearColor[0] = r;
        m_DrawDesc.ClearColor[1] = g;
        m_DrawDesc.ClearColor[2] = b;
        m_DrawDesc.ClearColor[3] = a;
    }

    void OffscreenRenderer::SetClearColor(const float rgba[4])
    {
        if (rgba == nullptr)
        {
            return;
        }
        m_DrawDesc.ClearColor[0] = rgba[0];
        m_DrawDesc.ClearColor[1] = rgba[1];
        m_DrawDesc.ClearColor[2] = rgba[2];
        m_DrawDesc.ClearColor[3] = rgba[3];
    }

    const RHITextureRef& OffscreenRenderer::RenderOnce()
    {
        static const RHITextureRef kEmpty;

        if (!m_Initialized || !m_Viewport)
        {
            return kEmpty;
        }
        SyncViewportDesc();
        if (m_DrawDesc.Scene == nullptr || m_DrawDesc.Camera == nullptr || m_DrawDesc.RenderTarget == nullptr)
        {
            return kEmpty;
        }

        if (m_ColorTarget)
        {
            if (m_ColorTarget->GetWidth() != m_Width || m_ColorTarget->GetHeight() != m_Height)
            {
                ME_LOG(
                    LogRender,
                    Error,
                    "OffscreenRenderer::RenderOnce: color target size {}x{} != offscreen {}x{}.",
                    m_ColorTarget->GetWidth(),
                    m_ColorTarget->GetHeight(),
                    m_Width,
                    m_Height);
                m_DrawDesc.ExternalSceneColor.reset();
                return kEmpty;
            }
            m_DrawDesc.ExternalSceneColor = m_ColorTarget->GetRHITextureShared();
            if (!m_DrawDesc.ExternalSceneColor)
            {
                ME_LOG(LogRender, Error, "OffscreenRenderer::RenderOnce: color target has no GPU texture.");
                return kEmpty;
            }
        }
        else
        {
            m_DrawDesc.ExternalSceneColor.reset();
        }

        // Sync camera aspect to the offscreen buffer.
        if (m_Width > 0 && m_Height > 0)
        {
            m_DrawDesc.Camera->m_AspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
            m_DrawDesc.Camera->UpdateProjectionMatrix();
            m_DrawDesc.Camera->UpdateViewProjMatrix();
        }

        m_Viewport->SetObservedScene(m_DrawDesc.Scene);
        RenderSystem::Get().SubmitSceneDraw(m_DrawDesc);
        return GetColorTexture();
    }

    const RHITextureRef& OffscreenRenderer::GetColorTexture() const
    {
        static const RHITextureRef kEmpty;
        if (m_ColorTarget && m_ColorTarget->GetRHITextureShared())
        {
            return m_ColorTarget->GetRHITextureShared();
        }
        return m_Viewport ? m_Viewport->GetColorTexture() : kEmpty;
    }

    const RHITextureRef& OffscreenRenderer::GetDepthTexture() const
    {
        static const RHITextureRef kEmpty;
        const SceneRenderTarget* rt = m_Viewport ? m_Viewport->GetRenderTarget() : nullptr;
        return rt ? rt->GetDepthTexture() : kEmpty;
    }

    RHI* OffscreenRenderer::ResolveRHI() const
    {
        return RenderSystem::Get().GetRHI();
    }
}
