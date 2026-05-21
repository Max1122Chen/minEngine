#include "SceneViewport.h"

#include "RenderCamera.h"
#include "RenderScene.h"
#include "SceneRenderTarget.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHITexture.h"

#include <cmath>

namespace minEngine
{
    void SceneViewport::Initialize(RHI* rhi, uint32_t width, uint32_t height)
    {
        m_Camera = std::make_shared<RenderCamera>();
        m_Camera->Initialize();

        m_RenderTarget = std::make_shared<SceneRenderTarget>();
        m_RenderTarget->Initialize(rhi, width, height);
    }

    void SceneViewport::Shutdown()
    {
        if (m_RenderTarget)
        {
            m_RenderTarget->Shutdown();
            m_RenderTarget.reset();
        }
        m_Camera.reset();
        m_ObservedScene = nullptr;
        m_HasPendingResize = false;
    }

    const std::shared_ptr<RHITexture2D>& SceneViewport::GetColorTexture() const
    {
        static const std::shared_ptr<RHITexture2D> s_Empty;
        if (!m_RenderTarget)
        {
            return s_Empty;
        }
        return m_RenderTarget->GetColorTexture();
    }

    Math::Vector2 SceneViewport::GetBufferSize() const
    {
        if (!m_RenderTarget)
        {
            return Math::Vector2(0.0f, 0.0f);
        }
        return Math::Vector2(static_cast<float>(m_RenderTarget->GetWidth()),
                             static_cast<float>(m_RenderTarget->GetHeight()));
    }

    void SceneViewport::RequestResizeByRatio(float widthRatio, float heightRatio)
    {
        if (!m_RenderTarget || widthRatio == 0.0f || heightRatio == 0.0f)
        {
            return;
        }

        const float epsilon = 0.0001f;
        if (std::abs(widthRatio - 1.0f) < epsilon && std::abs(heightRatio - 1.0f) < epsilon)
        {
            return;
        }

        m_PendingWidth = static_cast<uint32_t>(m_RenderTarget->GetWidth() * widthRatio);
        m_PendingHeight = static_cast<uint32_t>(m_RenderTarget->GetHeight() * heightRatio);
        m_HasPendingResize = true;
    }

    void SceneViewport::ApplyPendingResize(RHI* rhi)
    {
        if (!m_HasPendingResize || !m_RenderTarget || !rhi)
        {
            return;
        }

        m_RenderTarget->Resize(rhi, m_PendingWidth, m_PendingHeight);
        m_HasPendingResize = false;
    }

    SceneDrawDesc SceneViewport::BuildDrawDesc(SceneDrawFlags flags) const
    {
        SceneDrawDesc desc{};
        desc.Scene = m_ObservedScene;
        desc.Camera = m_Camera.get();
        desc.RenderTarget = m_RenderTarget.get();
        desc.Flags = flags;
        return desc;
    }
}
