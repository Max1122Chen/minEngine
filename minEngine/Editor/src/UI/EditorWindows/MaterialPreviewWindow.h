#pragma once

#include "Core.h"
#include "EditorViewportWindow.h"
#include "Viewport/MaterialPreviewViewportClient.h"

namespace minEngine
{
    class MaterialPreviewWindow final : public EditorViewportWindow
    {
    public:
        explicit MaterialPreviewWindow(Editor& editor)
            : EditorViewportWindow(editor, "material_editor_preview", "Material Preview")
        {
            SetOpen(false);
        }

        EditorWindowSuite GetWindowSuite() const override { return EditorWindowSuite::MaterialEditing; }

        void OnAttach() override;

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
    };
}
