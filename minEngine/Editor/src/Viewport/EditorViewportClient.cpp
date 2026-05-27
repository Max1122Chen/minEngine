#include "EditorViewportClient.h"

#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/RenderSystem.h"

namespace minEngine
{
    EditorViewportClient::EditorViewportClient(std::string debugName)
        : m_DebugName(std::move(debugName))
    {
    }

    EditorViewportClient::~EditorViewportClient()
    {
        ShutdownEditorSceneViewport();
    }

    void EditorViewportClient::BeginFrame(float deltaTime)
    {
        m_LastDeltaTime = deltaTime;
    }

    void EditorViewportClient::UpdateFrameState(const ViewportFrameState& frameState)
    {
        m_FrameState = frameState;
    }

    void EditorViewportClient::EndFrame()
    {
        SyncRenderTargetSize();
    }

    void EditorViewportClient::InitializeEditorSceneViewport(RHI* rhi, uint32_t width, uint32_t height)
    {
        if (m_SceneViewportInitialized || !rhi)
        {
            return;
        }

        m_SceneViewport.Initialize(rhi, width, height);
        m_SceneViewportInitialized = true;
    }

    void EditorViewportClient::ShutdownEditorSceneViewport()
    {
        if (!m_SceneViewportInitialized)
        {
            return;
        }

        m_SceneViewport.Shutdown();
        m_SceneViewportInitialized = false;
        m_LastRequestedWidth = 0;
        m_LastRequestedHeight = 0;
    }

    void EditorViewportClient::SyncSceneViewportRenderTargetSize()
    {
        const Math::Vector2 bufferSize = m_SceneViewport.GetBufferSize();

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
        const float targetHeightRatio =
            static_cast<float>(requestedHeight) / static_cast<float>(m_LastRequestedHeight);

        m_SceneViewport.RequestResizeByRatio(targetWidthRatio, targetHeightRatio);
        m_LastRequestedWidth = requestedWidth;
        m_LastRequestedHeight = requestedHeight;
    }

    void EditorViewportClient::SyncSceneViewportCameraAspect()
    {
        RenderCamera* camera = m_SceneViewport.GetCamera();
        if (!camera)
        {
            return;
        }

        const Vector2 bufferSize = m_SceneViewport.GetBufferSize();
        if (bufferSize.x > 0.0f && bufferSize.y > 0.0f)
        {
            camera->m_AspectRatio = bufferSize.x / bufferSize.y;
            camera->UpdateProjectionMatrix();
            camera->UpdateViewProjMatrix();
        }
    }

    bool EditorViewportClient::SubmitObservedScene(SceneDrawFlags flags)
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        m_SceneViewport.ApplyPendingResize(rhi);

        const SceneDrawDesc desc = m_SceneViewport.BuildDrawDesc(flags);
        if (!desc.Scene || !desc.Camera || !desc.RenderTarget)
        {
            return false;
        }

        RenderSystem::Get().SubmitSceneDraw(desc);
        return true;
    }

    void EditorViewportClient::SyncRenderTargetSize()
    {
        SyncSceneViewportRenderTargetSize();
    }
}
