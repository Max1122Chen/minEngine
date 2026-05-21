#include "MaterialPreviewViewportClient.h"

#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/SceneViewport.h"

namespace minEngine
{
    MaterialPreviewViewportClient::MaterialPreviewViewportClient(std::string debugName)
        : EditorViewportClient(std::move(debugName))
    {
    }

    MaterialPreviewViewportClient::~MaterialPreviewViewportClient()
    {
        m_Preview.Shutdown();
    }

    void MaterialPreviewViewportClient::InitializePreviewViewport(RHI* rhi, uint32_t width, uint32_t height)
    {
        if (m_PreviewInitialized || !rhi)
        {
            return;
        }

        m_Preview.Initialize(rhi, width, height);
        m_PreviewInitialized = true;
    }

    void MaterialPreviewViewportClient::BuildPreviewContent()
    {
        if (!m_PreviewInitialized)
        {
            ME_CORE_WARN("MaterialPreviewViewportClient: BuildPreviewContent called before viewport init.");
            return;
        }

        m_Preview.BuildPreviewScene();
    }

    void MaterialPreviewViewportClient::EndFrame()
    {
        if (!m_PreviewInitialized || !m_Preview.IsContentReady())
        {
            return;
        }

        SyncRenderTargetSize();
        SyncPreviewCameraAspect();
        m_Preview.RefreshRenderScene();

        RHI* rhi = RenderSystem::Get().GetRHI();
        SceneViewport& viewport = m_Preview.GetSceneViewport();
        viewport.ApplyPendingResize(rhi);

        const SceneDrawFlags flags = SceneDrawFlags::EnablePostProcess;
        const SceneDrawDesc desc = viewport.BuildDrawDesc(flags);
        if (desc.Scene && desc.Camera && desc.RenderTarget)
        {
            RenderSystem::Get().SubmitSceneDraw(desc);
        }
    }

    void MaterialPreviewViewportClient::SyncPreviewCameraAspect()
    {
        RenderCamera* camera = m_Preview.GetSceneViewport().GetCamera();
        if (!camera)
        {
            return;
        }

        const Vector2 bufferSize = m_Preview.GetSceneViewport().GetBufferSize();
        if (bufferSize.x > 0.0f && bufferSize.y > 0.0f)
        {
            camera->m_AspectRatio = bufferSize.x / bufferSize.y;
            camera->UpdateProjectionMatrix();
            camera->UpdateViewProjMatrix();
        }
    }

    void MaterialPreviewViewportClient::SyncRenderTargetSize()
    {
        if (!m_PreviewInitialized)
        {
            return;
        }

        SceneViewport& viewport = m_Preview.GetSceneViewport();
        const Math::Vector2 bufferSize = viewport.GetBufferSize();

        const uint32_t requestedWidth = m_FrameState.ImageSize.x > 0.0f
            ? static_cast<uint32_t>(m_FrameState.ImageSize.x)
            : (bufferSize.x > 0.0f ? static_cast<uint32_t>(bufferSize.x) : 1u);
        const uint32_t requestedHeight = m_FrameState.ImageSize.y > 0.0f
            ? static_cast<uint32_t>(m_FrameState.ImageSize.y)
            : (bufferSize.y > 0.0f ? static_cast<uint32_t>(bufferSize.y) : 1u);

        if (m_LastRequestedWidth == 0 || m_LastRequestedHeight == 0)
        {
            if (bufferSize.x > 0.0f && bufferSize.y > 0.0f)
            {
                m_LastRequestedWidth = static_cast<uint32_t>(bufferSize.x);
                m_LastRequestedHeight = static_cast<uint32_t>(bufferSize.y);
            }
            else
            {
                m_LastRequestedWidth = requestedWidth;
                m_LastRequestedHeight = requestedHeight;
            }
            return;
        }

        if (requestedWidth == m_LastRequestedWidth && requestedHeight == m_LastRequestedHeight)
        {
            return;
        }

        const float targetWidthRatio = static_cast<float>(requestedWidth) / static_cast<float>(m_LastRequestedWidth);
        const float targetHeightRatio = static_cast<float>(requestedHeight) / static_cast<float>(m_LastRequestedHeight);

        viewport.RequestResizeByRatio(targetWidthRatio, targetHeightRatio);
        m_LastRequestedWidth = requestedWidth;
        m_LastRequestedHeight = requestedHeight;
    }
}
