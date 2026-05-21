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

        RenderSystem& renderSystem = RenderSystem::Get();
        const std::shared_ptr<RHITexture2D>& sceneColor = renderSystem.GetSceneColorTexture();
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
        DrawGizmo(viewportClient);

        ImGui::End();

        viewportClient.EndFrame();
    }

    void ViewportWindow::DrawGizmo(EditorViewportClient& client)
    {
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::BeginFrame();
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetGizmoSizeClipSpace(0.15f);
        ImGuizmo::Enable(true);

        const ViewportFrameState& frameState = client.GetFrameState();
        ImGuizmo::SetRect(frameState.ImageMin.x, frameState.ImageMin.y, frameState.ImageSize.x, frameState.ImageSize.y);
        RenderCamera* mainCamera = RenderSystem::Get().GetMainCamera();
        Matrix4 view = mainCamera->GetViewMatrix();
        Matrix4 projection = mainCamera->GetProjectionMatrix();
        // ImGuizmo::DrawGrid(value_ptr(view), value_ptr(projection), value_ptr(Matrix4(1.0f)), 100.0f);
        // Update Gizmo state
        GizmoState& gizmoState = client.GetGizmoState();
        // Draw and manipulate gizmo based on current mode
        if (GameObject* selected = m_Editor.GetSelectedGameObject())
        {
            Matrix4 model = selected->GetTransform().ToMatrix();
            Matrix4 deltaMatrix;

            ImGuizmo::OPERATION operation;
            switch (gizmoState.mode)
            {
            case GizmoState::Mode::Translate:   operation = ImGuizmo::TRANSLATE; break;
            case GizmoState::Mode::Rotate:      operation = ImGuizmo::ROTATE;    break;
            case GizmoState::Mode::Scale:       operation = ImGuizmo::SCALE;     break;
            default:                            operation = ImGuizmo::TRANSLATE; break;
            }
            gizmoState.Hovering = ImGuizmo::IsOver();
            gizmoState.Using = ImGuizmo::IsUsing();
            // Currently we only support World mode, local mode is too complicated to implement.
            gizmoState.Manipulated = ImGuizmo::Manipulate(value_ptr(view), value_ptr(projection), operation, ImGuizmo::WORLD, value_ptr(model), value_ptr(deltaMatrix));
            client.SetInputBlockedByGizmo(gizmoState.Using || gizmoState.Hovering);
            if(gizmoState.Using && gizmoState.Manipulated)
            {
                // We will consume the delta in EditorViewClient and apply it to the selected GameObject later in the frame instead of applying it immediately here.
                // The "delta" below doesn't represent the actual delta transform in our logical world space. It is the delta in the render space. 
                // So when we apply it to the GameObject's transform later, we will need to apply it to the render transform of the Object. Then convert the updated render transform back to the logical transform and set it to the GO.
                Vector3 translation = Vector3(deltaMatrix[3]);
                Matrix3 rotMat = Matrix3(deltaMatrix);
                rotMat[0] = glm::normalize(rotMat[0]);
                rotMat[1] = glm::normalize(rotMat[1]);
                rotMat[2] = glm::normalize(rotMat[2]);
                glm::quat orientation = glm::quat_cast(deltaMatrix);
                Vector3 scale = Vector3(
                    glm::length(Vector3(deltaMatrix[0])),
                    glm::length(Vector3(deltaMatrix[1])),
                    glm::length(Vector3(deltaMatrix[2])));
                gizmoState.Delta = {
                    translation,
                    orientation,
                    scale
                };
            }
            
        }
    }

    Matrix4 ViewportWindow::CalculateViewMatrixForGizmo(const RenderCamera &camera) const
    {
        Matrix4 rotationMatrix = glm::mat4(1.0f);
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(camera.GetRotation().x), Vector3(1.0f, 0.0f, 0.0f));    // rotation x
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(camera.GetRotation().y), Vector3(0.0f, 1.0f, 0.0f));    // rotation y
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(camera.GetRotation().z), Vector3(0.0f, 0.0f, 1.0f));    // rotation z
        Vector3 forward = glm::normalize(rotationMatrix * Vector4(1.0f, 0.0f, 0.0f, 0.0f));
        Vector3 position = camera.GetPosition();
        return glm::lookAt(position, position + forward, Vector3(0.0f, 1.0f, 0.0f));
    }

    Matrix4 ViewportWindow::CalculateProjectionMatrixForGizmo(const RenderCamera &camera) const
    {
        return Matrix4();
    }

}