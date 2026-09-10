#include "SceneEditingViewportWindow.h"

#include "Shell/EditorContextHelpers.h"
#include "Shell/ViewportClientRegistry.h"
#include "UI/Chrome/ViewportPlayToolbar.h"
#include "Render/RenderCamera.h"
#include "Function/Framework/Components/ButtonComponent.h"
#include "Function/Framework/Components/CanvasComponent.h"
#include "Function/Framework/Components/WidgetComponent.h"
#include "Function/Framework/GameObject/GameObject.h"
#include "Function/Framework/Scene/Scene.h"
#include "Function/Render/ScreenUI/ScreenUICoords.h"
#include "Function/UI/UISystem.h"

#include <cstdio>

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
        if (m_Context.IsPlaying())
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 1.0f, 1.0f), "| ScreenUI hit debug (see viewport top-left)");
        }
    }

    void SceneEditingViewportWindow::SyncScreenUIPointerViewport(const ViewportFrameState& frameState)
    {
        if (!UISystem::HasInstance())
        {
            return;
        }

        ScreenUIPointerViewportContext context;
        context.ImageMin = frameState.ImageMin;
        context.ImageSize = frameState.ImageSize;
        context.bValid = frameState.ImageSize.x > 0.0f && frameState.ImageSize.y > 0.0f;
        UISystem::Get().SetPointerViewportContext(context);
        UISystem::Get().PollPointer();
    }

    void SceneEditingViewportWindow::DrawScreenUIPointerDebugOverlay(const ViewportFrameState& frameState)
    {
        // Always draw during Play so missing routing / UISystem is visible (acceptance aid).
        const bool hasUI = UISystem::HasInstance();
        const bool routing = hasUI && UISystem::Get().IsPointerRoutingEnabled();

        const char* hoveredName = "(none)";
        const char* pressedName = "(none)";
        bool click = false;
        bool pointerDown = false;
        bool ctxValid = false;
        bool buttonOnClicked = false;
        float ctxW = 0.0f;
        float ctxH = 0.0f;
        WidgetComponent* hoveredWidget = nullptr;
        ButtonComponent* hoveredButton = nullptr;

        if (hasUI)
        {
            const ScreenUIPointerState& pointer = UISystem::Get().GetPointerState();
            const ScreenUIPointerViewportContext& viewport = UISystem::Get().GetPointerViewportContext();
            click = pointer.bClickThisFrame;
            buttonOnClicked = pointer.bButtonOnClickedThisFrame;
            pointerDown = pointer.bPointerDown;
            ctxValid = viewport.bValid;
            ctxW = viewport.ImageSize.x;
            ctxH = viewport.ImageSize.y;
            hoveredWidget = pointer.Hovered;
            if (pointer.Hovered != nullptr && pointer.Hovered->GetOwner() != nullptr)
            {
                hoveredName = pointer.Hovered->GetOwner()->GetName().c_str();
                const std::vector<std::shared_ptr<ButtonComponent>> buttons =
                    pointer.Hovered->GetOwner()->GetComponentsOfType<ButtonComponent>();
                if (!buttons.empty() && buttons[0])
                {
                    hoveredButton = buttons[0].get();
                }
            }
            if (pointer.Pressed != nullptr && pointer.Pressed->GetOwner() != nullptr)
            {
                pressedName = pointer.Pressed->GetOwner()->GetName().c_str();
            }
        }

        if (buttonOnClicked)
        {
            ++m_ScreenUIButtonOnClickedCount;
        }

        char line[320];
        std::snprintf(
            line,
            sizeof(line),
            "ScreenUI [%s] Hover:%s Pressed:%s Click:%s Ctx:%s (%.0fx%.0f)",
            !hasUI ? "NO UISystem" : (routing ? "routing ON" : "routing OFF"),
            hoveredName,
            pressedName,
            click ? "YES" : "no",
            ctxValid ? "ok" : "INVALID",
            ctxW,
            ctxH);

        char buttonLine[240];
        if (hoveredButton != nullptr)
        {
            std::snprintf(
                buttonLine,
                sizeof(buttonLine),
                "Button Hover:yes Interactable:%s OnClickedEdge:%s Total:%d (watch Image tint)",
                hoveredButton->IsInteractable() ? "yes" : "NO",
                buttonOnClicked ? "YES" : "no",
                m_ScreenUIButtonOnClickedCount);
        }
        else
        {
            std::snprintf(
                buttonLine,
                sizeof(buttonLine),
                "Button Hover:no OnClickedEdge:%s Total:%d (add ButtonComponent on Widget GO)",
                buttonOnClicked ? "YES" : "no",
                m_ScreenUIButtonOnClickedCount);
        }

        // Foreground list avoids clip from Image / child regions.
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        const ImVec2 textPos(frameState.ImageMin.x + 8.0f, frameState.ImageMin.y + 8.0f);
        const ImVec2 textSize = ImGui::CalcTextSize(line);
        const ImVec2 buttonTextSize = ImGui::CalcTextSize(buttonLine);
        const float boxWidth = (textSize.x > buttonTextSize.x ? textSize.x : buttonTextSize.x) + 6.0f;
        const float boxHeight = textSize.y + buttonTextSize.y + 8.0f;
        drawList->AddRectFilled(
            ImVec2(textPos.x - 6.0f, textPos.y - 4.0f),
            ImVec2(textPos.x + boxWidth, textPos.y + boxHeight),
            IM_COL32(0, 0, 0, 200));
        drawList->AddRect(
            ImVec2(textPos.x - 6.0f, textPos.y - 4.0f),
            ImVec2(textPos.x + boxWidth, textPos.y + boxHeight),
            IM_COL32(255, 220, 0, 255),
            0.0f,
            0,
            2.0f);
        drawList->AddText(textPos, IM_COL32(120, 255, 255, 255), line);
        drawList->AddText(
            ImVec2(textPos.x, textPos.y + textSize.y + 2.0f),
            buttonOnClicked ? IM_COL32(255, 220, 80, 255) : IM_COL32(180, 255, 160, 255),
            buttonLine);

        if (!routing || hoveredWidget == nullptr)
        {
            return;
        }

        GameObject* owner = hoveredWidget->GetOwner();
        CanvasComponent* canvas = CanvasComponent::FindOwningCanvas(owner);
        if (canvas == nullptr)
        {
            return;
        }

        const Vector2 ref = canvas->GetReferenceResolution();
        const ScreenUICoords::LetterboxMapping mapping = ScreenUICoords::MakeLetterboxMapping(
            ref.x, ref.y, frameState.ImageSize.x, frameState.ImageSize.y);

        const UIRect& rect = hoveredWidget->GetComputedRect();
        const Vector2 topLeftVp = mapping.MapPoint(rect.TopLeft);
        const Vector2 sizeVp = mapping.MapSize(rect.Size);
        const ImVec2 screenMin(
            frameState.ImageMin.x + topLeftVp.x,
            frameState.ImageMin.y + topLeftVp.y);
        const ImVec2 screenMax(screenMin.x + sizeVp.x, screenMin.y + sizeVp.y);

        const ImU32 outlineColor = pointerDown ? IM_COL32(255, 180, 40, 255) : IM_COL32(80, 255, 120, 255);
        drawList->AddRect(screenMin, screenMax, outlineColor, 0.0f, 0, 3.0f);
    }

    void SceneEditingViewportWindow::OnPostSceneImageDraw(EditorViewportClient& client,
                                                          const ViewportFrameState& frameState)
    {
        // Play view is game camera / PIE world — no editor transform gizmo.
        if (m_Context.IsPlaying())
        {
            SceneEditingViewportClient& sceneClient = static_cast<SceneEditingViewportClient&>(client);
            GizmoState& gizmoState = sceneClient.GetGizmoState();
            gizmoState.Hovering = false;
            gizmoState.Using = false;
            gizmoState.Manipulated = false;
            gizmoState.HasResultWorldMatrix = false;
            sceneClient.SetInputBlockedByGizmo(false);

            SyncScreenUIPointerViewport(frameState);
            DrawScreenUIPointerDebugOverlay(frameState);
            return;
        }

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
            Matrix4 model = selected->GetWorldTransform().ToMatrix();
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
            gizmoState.HasResultWorldMatrix = false;
            gizmoState.Manipulated = ImGuizmo::Manipulate(
                value_ptr(view), value_ptr(projection), operation, ImGuizmo::WORLD, value_ptr(model), value_ptr(deltaMatrix));
            client.SetInputBlockedByGizmo(gizmoState.Using || gizmoState.Hovering);

            if (gizmoState.Using && gizmoState.Manipulated)
            {
                gizmoState.HasResultWorldMatrix = true;
                gizmoState.ResultWorldMatrix = model;
            }
        }
    }
}
