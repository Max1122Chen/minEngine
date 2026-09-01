#include "EditorViewportWindow.h"

#include "Runtime/Function/Render/RHI/RHIClipSpaceCapabilities.h"
#include "Shell/EditorInputHub.h"
#include "Shell/ViewportClientRegistry.h"
#include "UI/Appearance/EditorWindowTypography.h"

namespace minEngine
{
    void EditorViewportWindow::OnAttach()
    {
        GetOrCreateViewportClient();
    }

    void EditorViewportWindow::OnDetach()
    {
        m_Context.GetViewportRegistry().RemoveViewportClient(m_Id);
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

bool EditorViewportWindow::DrawSceneColorImage(EditorViewportClient& viewportClient, ViewportFrameState& outFrameState)
{
    RHI* rhi = RenderSystem::HasInstance() ? RenderSystem::Get().GetRHI() : nullptr;
    m_PinnedFrameTexture.reset();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    const ViewportImageLayout layout =
        viewportClient.ComputeViewportImageLayout(Vector2(avail.x, avail.y));

    outFrameState.Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    outFrameState.Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    outFrameState.ContentSize = { avail.x, avail.y };
    outFrameState.ImageMin = { cursorPos.x + layout.Offset.x, cursorPos.y + layout.Offset.y };
    outFrameState.ImageSize = layout.Size;

    m_PinnedFrameTexture = GetDisplayColorTexture();
    if (!m_PinnedFrameTexture
        || m_PinnedFrameTexture->GetDesc().Width == 0
        || m_PinnedFrameTexture->GetDesc().Height == 0)
    {
        m_PinnedImGuiTexture.Reset(rhi);
        ImGui::TextWrapped("Scene color texture is not ready.");
        return false;
    }

    const ImTextureID textureID = m_PinnedImGuiTexture.Pin(rhi, m_PinnedFrameTexture.get());
    if (textureID == ImTextureID_Invalid)
    {
        m_PinnedImGuiTexture.Reset(rhi);
        ImGui::TextWrapped("Scene color texture is not ready.");
        return false;
    }

    ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + layout.Offset.x, cursorPos.y + layout.Offset.y));
    const RHIImGuiSceneColorUv sceneUv = GetImGuiSceneColorUv();
    const ImVec2 uv0(sceneUv.U0, sceneUv.V0);
    const ImVec2 uv1(sceneUv.U1, sceneUv.V1);
    ImGui::Image(textureID, ImVec2(layout.Size.x, layout.Size.y), uv0, uv1);

    const ImVec2 imageMin = ImGui::GetItemRectMin();
    const ImVec2 imageSize = ImGui::GetItemRectSize();
    outFrameState.ImageMin = { imageMin.x, imageMin.y };
    outFrameState.ImageSize = { imageSize.x, imageSize.y };
    return true;
}

    void EditorViewportWindow::OnDraw()
    {
        EditorViewportClient& viewportClient = GetViewportClient();
        viewportClient.BeginFrame(m_Context.GetLastDeltaTime());

        if (!EditorWindowTypography::BeginPanel(m_Context, m_Title.c_str(), nullptr, GetViewportWindowFlags()))
        {
            return;
        }

    ViewportFrameState frameState{};
    const bool drewSceneImage = DrawSceneColorImage(viewportClient, frameState);
    viewportClient.UpdateFrameState(frameState);

    EditorInputHub& inputHub = m_Context.GetInputHub();
    if (frameState.Focused)
    {
        inputHub.SetFocusedViewportClient(&viewportClient);
    }
    else if (inputHub.GetFocusedViewportClient() == &viewportClient)
    {
        inputHub.SetFocusedViewportClient(nullptr);
    }

    if (drewSceneImage)
    {
        OnDrawViewportOverlay(viewportClient, frameState);
    }

    ImGui::End();
        viewportClient.EndFrame();
    }
}
