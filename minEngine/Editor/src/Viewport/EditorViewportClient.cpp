#include "EditorViewportClient.h"

namespace minEngine
{
    EditorViewportClient::EditorViewportClient(std::string debugName)
        : m_DebugName(std::move(debugName))
    {
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

    void EditorViewportClient::SyncRenderTargetSize()
    {
    }
}
