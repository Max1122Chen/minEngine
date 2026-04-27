#include "ViewportWindow.h"

namespace minEngine
{
    void ViewportWindow::OnDraw()
    {
        ImGuiWindowFlags viewportFlags = ImGuiWindowFlags_NoScrollbar |
                                            ImGuiWindowFlags_NoScrollWithMouse |
                                            ImGuiWindowFlags_NoCollapse;

        EditorViewportClient& viewportClient = GetViewportClient();
        viewportClient.BeginFrame(m_Editor.lastDeltaTime);

        ImGui::Begin(m_Title.c_str(), nullptr, viewportFlags);

        ImVec2 avail = ImGui::GetContentRegionAvail();

        const auto& renderSystem = RuntimeGlobalContext::Get().m_RenderSystem;
        if (!renderSystem)
        {
            ImGui::TextWrapped("RenderSystem is not ready.");
            ImGui::End();
            viewportClient.EndFrame();
            return;
        }

        const std::shared_ptr<RHITexture2D>& sceneColor = renderSystem->GetSceneColorTexture();
        if (!sceneColor)
        {
            ImGui::TextWrapped("Scene color texture is not ready.");
            ImGui::End();
            viewportClient.EndFrame();
            return;
        }

        // Draw the scene color texture to the viewport
        const ImTextureID textureID = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(sceneColor->GetID()));
        ImGui::Image(textureID, avail, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        const ImVec2 ImageMin = ImGui::GetItemRectMin();
        const ImVec2 ImageSize = ImGui::GetItemRectSize();

        ViewportFrameState frameState;
        frameState.Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        frameState.Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        frameState.ContentSize = { avail.x, avail.y };
        frameState.ImageMin = { ImageMin.x, ImageMin.y };
        frameState.ImageSize = { ImageSize.x, ImageSize.y };
        viewportClient.UpdateFrameState(frameState);

        const float deltaTime = viewportClient.GetLastDeltaTime();
        const float fps = (deltaTime > 0.0001f) ? (1.0f / deltaTime) : 0.0f;
        const std::string sceneName = "TODO: get correct scene name later"; // TODO: implement getting current scene name logic later

        m_OverlayConfig.expandedSize = ImVec2(std::max(220.0f, std::min(420.0f, ImageSize.x * 0.46f)), 96.0f);
        UI::ClampOverlayOffset(m_OverlayState, m_OverlayConfig, ImageSize);
        {
            const std::string overlayId = m_Id + "_overlay";
            ImGui::SetCursorScreenPos(UI::GetOverlayScreenPos(m_OverlayState, ImageMin));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.10f, 0.13f, 0.82f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.27f, 0.37f, 0.50f, 0.95f));
            if (ImGui::BeginChild(overlayId.c_str(), UI::GetOverlaySize(m_OverlayState, m_OverlayConfig), true,
                                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav))
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));
                if (m_OverlayState.collapsed)
                {
                    if (ImGui::Button(">"))
                    {
                        m_OverlayState.collapsed = false;
                    }

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("Expand overlay");
                    }
                }
                else
                {
                    ImGui::TextUnformatted("Overlay");
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 24.0f);
                    if (ImGui::Button("_"))
                    {
                        m_OverlayState.collapsed = true;
                    }

                    ImGui::Separator();
                    ImGui::TextWrapped("Scene: %s", sceneName.c_str());
                    ImGui::Text("FPS: %.1f", fps);
                    ImGui::Text("Frame: %.2f ms", deltaTime * 1000.0f);
                    ImGui::Text("Viewport: %.0f x %.0f", avail.x, avail.y);
                }

                UI::HandleOverlayDragging(m_OverlayState, m_OverlayConfig, ImageSize);
                ImGui::PopStyleVar();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
        }

        // Draw gizmos
        DrawGizmo(frameState);

        ImGui::End();

        viewportClient.EndFrame();
    }

    void ViewportWindow::DrawGizmo(ViewportFrameState& frameState)
    {
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetGizmoSizeClipSpace(0.15f);

        ImGuizmo::SetRect(frameState.ImageMin.x, frameState.ImageMin.y, frameState.ImageSize.x, frameState.ImageSize.y);
        if (GameObject* selected = m_Editor.GetSelectedGameObject())
        {
            RenderCamera* mainCamera = RuntimeGlobalContext::Get().m_RenderSystem->GetMainCamera();
            Matrix4 view = mainCamera->GetViewMatrix();
            Matrix4 projection = mainCamera->GetProjectionMatrix();
            Matrix4 model = selected->GetTransform().ToMatrix();

            ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), ImGuizmo::TRANSLATE, ImGuizmo::WORLD, glm::value_ptr(model));
        }
    }
}