#include "SceneEditingViewportWindow.h"

#include "Shell/EditorContextHelpers.h"
#include "Shell/ViewportClientRegistry.h"
#include "UI/Chrome/ViewportPlayToolbar.h"
#include "Render/RenderCamera.h"
#include "Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    EditorViewportClient& SceneEditingViewportWindow::GetOrCreateViewportClient()
    {
        return m_Context.GetViewportRegistry().GetOrCreateSceneEditingViewportClient(m_Id, m_Title);
    }

    const RHITextureRef& SceneEditingViewportWindow::GetDisplayColorTexture() const
    {
        return GetSceneEditingViewportClient().GetSceneViewport().GetColorTexture();
    }

    void SceneEditingViewportWindow::DrawViewportToolbarRow()
    {
        ViewportPlayToolbar::DrawToolbarRow(m_Context);
    }

    void SceneEditingViewportWindow::OnPostSceneImageDraw(EditorViewportClient& client,
                                                          const ViewportFrameState& /*frameState*/)
    {
        DrawGizmo(static_cast<SceneEditingViewportClient&>(client));
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
