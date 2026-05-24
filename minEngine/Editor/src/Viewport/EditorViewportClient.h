#pragma once

#include "Core.h"
#include "Viewport/EditorViewportTypes.h"

namespace minEngine
{
    class IEditorContext;

    /** Shared frame lifecycle for all editor viewport panels. */
    class EditorViewportClient
    {
    public:
        explicit EditorViewportClient(std::string debugName = "Viewport");
        virtual ~EditorViewportClient() = default;

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

    protected:
        virtual void SyncRenderTargetSize();

        IEditorContext* m_Context = nullptr;
        std::string m_DebugName;
        ViewportFrameState m_FrameState;
        float m_LastDeltaTime = 0.0f;

        uint32_t m_LastRequestedWidth = 0;
        uint32_t m_LastRequestedHeight = 0;
    };
}
