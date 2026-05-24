#pragma once

#include "Core.h"

#include "Material/MaterialEditor.h"
#include "UI/EditorWindows/EditorViewportWindow.h"
#include "Viewport/MaterialPreviewViewportClient.h"

namespace minEngine
{
    class MaterialPreviewWindow final : public EditorViewportWindow
    {
    public:
        explicit MaterialPreviewWindow(IEditorContext& context)
            : EditorViewportWindow(context, "material_editor_preview", "Material Preview")
        {
            SetOpen(false);
        }

        std::string_view GetOwnerModuleId() const override { return MaterialEditor::kModuleId; }

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
