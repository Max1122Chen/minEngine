#pragma once

#include "Core.h"

#include "imgui.h"
#include "ImGuizmo.h"

#include "SubEditor/Scene/SceneEditor.h"
#include "UI/EditorWindows/EditorViewportWindow.h"
#include "SubEditor/Scene/SceneEditingViewportClient.h"

#include <string>
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
        const RHITextureRef& GetDisplayColorTexture() const override;
        bool WantsViewportToolbarRow() const override { return true; }
        void DrawViewportToolbarRow() override;
        void OnPostSceneImageDraw(EditorViewportClient& client, const ViewportFrameState& frameState) override;

    private:
        void DrawGizmo(SceneEditingViewportClient& client);
    };
}
