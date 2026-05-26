#include "SceneEditingViewportWindow.h"

#include "Shell/EditorContextHelpers.h"
#include "UI/Appearance/EditorThemeScope.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTheme.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"
#include "Shell/ViewportClientRegistry.h"
#include "Render/RenderCamera.h"
#include "Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    EditorViewportClient& SceneEditingViewportWindow::GetOrCreateViewportClient()
    {
        return m_Context.GetViewportRegistry().GetOrCreateSceneEditingViewportClient(m_Id, m_Title);
    }

    const std::shared_ptr<RHITexture2D>& SceneEditingViewportWindow::GetDisplayColorTexture() const
    {
        return GetSceneEditingViewportClient().GetSceneViewport().GetColorTexture();
    }

    void SceneEditingViewportWindow::OnDrawViewportOverlay(EditorViewportClient& client,
                                                           const ViewportFrameState& frameState)
    {
        SceneEditingViewportClient& sceneClient = static_cast<SceneEditingViewportClient&>(client);

        const float deltaTime = sceneClient.GetLastDeltaTime();
        const float fps = (deltaTime > 0.0001f) ? (1.0f / deltaTime) : 0.0f;
        const std::string sceneName = "TODO: get correct scene name later";

        const ImVec2 imageSize(frameState.ImageSize.x, frameState.ImageSize.y);
        const ImVec2 imageMin(frameState.ImageMin.x, frameState.ImageMin.y);

        m_OverlayConfig.expandedSize = ImVec2(std::max(220.0f, std::min(420.0f, imageSize.x * 0.46f)), 96.0f);
        UI::ClampOverlayOffset(m_OverlayState, m_OverlayConfig, imageSize);

        const std::string overlayId = m_Id + "_overlay";
        ImGui::SetCursorScreenPos(UI::GetOverlayScreenPos(m_OverlayState, imageMin));
        if (ImGui::BeginChild(overlayId.c_str(), UI::GetOverlaySize(m_OverlayState, m_OverlayConfig), true,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav))
        {
            EditorThemeScope overlayTheme = EditorWindowTheme::PanelOverlay(m_Context.GetEditorAppearance());
            EditorTypographyScope captionTypography(
                m_Context.GetEditorAppearance(),
                EditorTypographyRole::Caption);
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
                ImGui::Text("Viewport: %.0f x %.0f", frameState.ContentSize.x, frameState.ContentSize.y);
            }

            UI::HandleOverlayDragging(m_OverlayState, m_OverlayConfig, imageSize);
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();

        DrawGizmo(sceneClient);
    }

    void SceneEditingViewportWindow::DrawGizmo(SceneEditingViewportClient& client)
    {
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::BeginFrame();
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetGizmoSizeClipSpace(0.15f);
        ImGuizmo::Enable(true);

        const ViewportFrameState& frameState = client.GetFrameState();
        ImGuizmo::SetRect(
            frameState.ImageMin.x,
            frameState.ImageMin.y,
            frameState.ImageSize.x > 0.0f ? frameState.ImageSize.x : 1.0f,
            frameState.ImageSize.y > 0.0f ? frameState.ImageSize.y : 1.0f);

        RenderCamera* viewportCamera = client.GetSceneViewport().GetCamera();
        if (!viewportCamera)
        {
            return;
        }

        Matrix4 view = viewportCamera->GetViewMatrix();
        Matrix4 projection = viewportCamera->GetProjectionMatrix();

        GizmoState& gizmoState = client.GetGizmoState();
        SceneEditor* sceneEditor = GetSceneEditor(&m_Context);
        if (GameObject* selected = sceneEditor ? sceneEditor->GetSelectedGameObject() : nullptr)
        {
            Matrix4 model = selected->GetTransform().ToMatrix();
            Matrix4 deltaMatrix;

            ImGuizmo::OPERATION operation;
            switch (gizmoState.mode)
            {
            case GizmoState::Mode::Translate:
                operation = ImGuizmo::TRANSLATE;
                break;
            case GizmoState::Mode::Rotate:
                operation = ImGuizmo::ROTATE;
                break;
            case GizmoState::Mode::Scale:
                operation = ImGuizmo::SCALE;
                break;
            default:
                operation = ImGuizmo::TRANSLATE;
                break;
            }

            gizmoState.Hovering = ImGuizmo::IsOver();
            gizmoState.Using = ImGuizmo::IsUsing();
            gizmoState.Manipulated = ImGuizmo::Manipulate(
                value_ptr(view), value_ptr(projection), operation, ImGuizmo::WORLD, value_ptr(model), value_ptr(deltaMatrix));
            client.SetInputBlockedByGizmo(gizmoState.Using || gizmoState.Hovering);

            if (gizmoState.Using && gizmoState.Manipulated)
            {
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
                gizmoState.Delta = { translation, orientation, scale };
            }
        }
    }
}
