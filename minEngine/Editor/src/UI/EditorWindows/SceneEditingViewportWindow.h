#pragma once

#include "Core.h"

#include "imgui.h"
#include "ImGuizmo.h"

#include "Scene/SceneEditor.h"
#include "UI/EditorWindows/EditorViewportWindow.h"
#include "UI/Widgets/DraggableOverlay.h"
#include "Viewport/SceneEditingViewportClient.h"

#include <algorithm>
#include <utility>

namespace minEngine
{
    class SceneEditingViewportWindow final : public EditorViewportWindow
    {
    public:
        explicit SceneEditingViewportWindow(IEditorContext& context,
                                            std::string id = "scene_editing_viewport",
                                            std::string title = "Viewport")
            : EditorViewportWindow(context, std::move(id), std::move(title))
        {
        }

        std::string_view GetOwnerModuleId() const override { return SceneEditor::kModuleId; }

        SceneEditingViewportClient& GetSceneEditingViewportClient()
        {
            return static_cast<SceneEditingViewportClient&>(GetViewportClient());
        }

        const SceneEditingViewportClient& GetSceneEditingViewportClient() const
        {
            return static_cast<const SceneEditingViewportClient&>(GetViewportClient());
        }

    protected:
        EditorViewportClient& GetOrCreateViewportClient() override;
        const std::shared_ptr<RHITexture2D>& GetDisplayColorTexture() const override;
        void OnDrawViewportOverlay(EditorViewportClient& client, const ViewportFrameState& frameState) override;

    private:
        void DrawGizmo(SceneEditingViewportClient& client);

        UI::DraggableOverlayState m_OverlayState;
        UI::DraggableOverlayConfig m_OverlayConfig;
    };
}
