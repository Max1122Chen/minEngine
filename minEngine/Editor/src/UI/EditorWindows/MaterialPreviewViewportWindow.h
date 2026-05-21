#pragma once

#include "Core.h"
#include "EditorViewportWindow.h"
#include "Viewport/MaterialPreviewViewportClient.h"

namespace minEngine
{
    /** Debug / P5 panel: second RT for material preview (no Gizmo). */
    class MaterialPreviewViewportWindow final : public EditorViewportWindow
    {
    public:
        explicit MaterialPreviewViewportWindow(Editor& editor,
                                               std::string id = "material_preview_viewport",
                                               std::string title = "Material Preview")
            : EditorViewportWindow(editor, std::move(id), std::move(title))
        {
            SetOpen(true);
        }

        MaterialPreviewViewportClient& GetMaterialPreviewViewportClient()
        {
            return static_cast<MaterialPreviewViewportClient&>(GetViewportClient());
        }

        const MaterialPreviewViewportClient& GetMaterialPreviewViewportClient() const
        {
            return static_cast<const MaterialPreviewViewportClient&>(GetViewportClient());
        }

    protected:
        EditorViewportClient& GetOrCreateViewportClient() override;
        const std::shared_ptr<RHITexture2D>& GetDisplayColorTexture() const override;
        void OnDrawViewportOverlay(EditorViewportClient& client, const ViewportFrameState& frameState) override;
    };
}
