#include "EditorViewportClient.h"

#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/RenderSystem.h"

#include <algorithm>

namespace minEngine
{
    namespace
    {
        /**
         * Marks a write of state the render role reads from the frame it is executing.
         *
         * Called before the write, from the game role: while a frame is in flight the two roles
         * overlap on exactly this state, and a silent overlap is what lets a camera update land in
         * the middle of a recorded frame without anybody noticing.
         */
        void NoteRenderSideWrite(const char* what)
        {
            if (TaskSystem::HasInstance())
            {
                TaskSystem::Get().NoteGameRoleRenderWrite(what);
            }
        }
    }

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

    ViewportImageLayout EditorViewportClient::ComputeViewportImageLayout(const Vector2& contentSize) const
    {
        ViewportImageLayout layout;

        const float contentWidth = std::max(contentSize.x, 1.0f);
        const float contentHeight = std::max(contentSize.y, 1.0f);

        if (!m_AspectPolicy.bKeepAspect)
        {
            layout.Size = Vector2(contentWidth, contentHeight);
            return layout;
        }

        const float targetAspect = std::max(m_AspectPolicy.TargetAspect, 0.0001f);
        const float contentAspect = contentWidth / contentHeight;

        if (m_AspectPolicy.FitMode == ViewportImageFitMode::Cover)
        {
            if (contentAspect > targetAspect)
            {
                layout.Size.x = contentWidth;
                layout.Size.y = contentWidth / targetAspect;
            }
            else
            {
                layout.Size.y = contentHeight;
                layout.Size.x = contentHeight * targetAspect;
            }
        }
        else
        {
            if (contentAspect > targetAspect)
            {
                layout.Size.y = contentHeight;
                layout.Size.x = contentHeight * targetAspect;
            }
            else
            {
                layout.Size.x = contentWidth;
                layout.Size.y = contentWidth / targetAspect;
            }
        }

        layout.Offset.x = std::max(0.0f, (contentWidth - layout.Size.x) * 0.5f);
        layout.Offset.y = std::max(0.0f, (contentHeight - layout.Size.y) * 0.5f);
        return layout;
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
            // This camera is not private state: it travels inside the draw desc a queued view intent
            // carries, so a frame still recording from an earlier intent reads it through that
            // pointer. Recorded before the write, and never prevented - a viewport stuck at the old
            // aspect is a visible bug, a recorded overlap is a debuggable one.
            NoteRenderSideWrite("the viewport camera's aspect and matrices");

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

        // Handing the desc over is the hand-over of everything it points at: the scene, the camera
        // and the target become render-role reads the moment this call enqueues the intent.
        NoteRenderSideWrite("a view intent");

        RenderSystem::Get().SubmitSceneDraw(desc);
        return true;
    }

    void EditorViewportClient::SyncRenderTargetSize()
    {
        SyncSceneViewportRenderTargetSize();
    }
}
