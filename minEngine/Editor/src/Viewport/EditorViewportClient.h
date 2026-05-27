#pragma once

#include "Core.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"
#include "Runtime/Function/Render/SceneViewport.h"
#include "Viewport/EditorViewportTypes.h"

namespace minEngine
{
    class IEditorContext;
    class RHI;

    /** Shared frame lifecycle for editor viewport panels. Owns SceneViewport (RT + scene bridge). */
    class EditorViewportClient
    {
    public:
        explicit EditorViewportClient(std::string debugName = "Viewport");
        virtual ~EditorViewportClient();

        void SetEditorContext(IEditorContext* context) { m_Context = context; }
        IEditorContext* GetEditorContext() const { return m_Context; }

        virtual void BeginFrame(float deltaTime);
        void UpdateFrameState(const ViewportFrameState& frameState);
        virtual void EndFrame();

        const ViewportFrameState& GetFrameState() const { return m_FrameState; }
        bool IsHovered() const { return m_FrameState.Hovered; }
        bool IsFocused() const { return m_FrameState.Focused; }
        float GetLastDeltaTime() const { return m_LastDeltaTime; }
        const std::string& GetDebugName() const { return m_DebugName; }

        SceneViewport& GetSceneViewport() { return m_SceneViewport; }
        const SceneViewport& GetSceneViewport() const { return m_SceneViewport; }
        bool IsSceneViewportInitialized() const { return m_SceneViewportInitialized; }

        void InitializeEditorSceneViewport(RHI* rhi, uint32_t width, uint32_t height);

    protected:
        void ShutdownEditorSceneViewport();

        void SyncSceneViewportRenderTargetSize();
        void SyncSceneViewportCameraAspect();
        bool SubmitObservedScene(SceneDrawFlags flags);

        virtual void SyncRenderTargetSize();

        SceneViewport m_SceneViewport;
        bool m_SceneViewportInitialized = false;

        IEditorContext* m_Context = nullptr;
        std::string m_DebugName;
        ViewportFrameState m_FrameState;
        float m_LastDeltaTime = 0.0f;

        uint32_t m_LastRequestedWidth = 0;
        uint32_t m_LastRequestedHeight = 0;
    };
}
