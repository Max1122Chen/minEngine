#include "EditorViewportWindow.h"

#include "Editor.h"

namespace minEngine
{
    void EditorViewportWindow::OnAttach()
    {
        GetOrCreateViewportClient();
    }

    void EditorViewportWindow::OnDetach()
    {
        m_Editor.RemoveViewportClient(m_Id);
    }

    EditorViewportClient& EditorViewportWindow::GetViewportClient()
    {
        return GetOrCreateViewportClient();
    }

    const EditorViewportClient& EditorViewportWindow::GetViewportClient() const
    {
        return const_cast<EditorViewportWindow*>(this)->GetOrCreateViewportClient();
    }

    ImGuiWindowFlags EditorViewportWindow::GetViewportWindowFlags() const
    {
        return ImGuiWindowFlags_NoScrollbar |
               ImGuiWindowFlags_NoScrollWithMouse |
               ImGuiWindowFlags_NoCollapse;
    }

    void EditorViewportWindow::OnDrawViewportOverlay(EditorViewportClient& /*client*/,
                                                   const ViewportFrameState& /*frameState*/)
    {
    }

bool EditorViewportWindow::DrawSceneColorImage(ViewportFrameState& outFrameState)
{
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    outFrameState.Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    outFrameState.Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    outFrameState.ContentSize = { avail.x, avail.y };
    outFrameState.ImageMin = { cursorPos.x, cursorPos.y };
    outFrameState.ImageSize = { avail.x, avail.y };

    const std::shared_ptr<RHITexture2D>& sceneColor = GetDisplayColorTexture();
    if (!sceneColor || sceneColor->GetWidth() == 0 || sceneColor->GetHeight() == 0)
    {
        ImGui::TextWrapped("Scene color texture is not ready.");
        return false;
    }

    const ImTextureID textureID = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(sceneColor->GetID()));
    ImGui::Image(textureID, avail, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

    const ImVec2 imageMin = ImGui::GetItemRectMin();
    const ImVec2 imageSize = ImGui::GetItemRectSize();
    outFrameState.ImageMin = { imageMin.x, imageMin.y };
    outFrameState.ImageSize = { imageSize.x, imageSize.y };
    return true;
}

    void EditorViewportWindow::OnDraw()
    {
        EditorViewportClient& viewportClient = GetViewportClient();
        viewportClient.BeginFrame(m_Editor.lastDeltaTime);

        ImGui::Begin(m_Title.c_str(), nullptr, GetViewportWindowFlags());

    ViewportFrameState frameState{};
    const bool drewSceneImage = DrawSceneColorImage(frameState);
    viewportClient.UpdateFrameState(frameState);
    if (drewSceneImage)
    {
        OnDrawViewportOverlay(viewportClient, frameState);
    }

    ImGui::End();
        viewportClient.EndFrame();
    }
}
