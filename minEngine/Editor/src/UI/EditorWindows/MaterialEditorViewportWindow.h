#pragma once

#include "Core.h"

#include "SubEditor/Material/MaterialEditor.h"
#include "UI/EditorWindows/EditorViewportWindow.h"
#include "SubEditor/Material/MaterialEditorViewportClient.h"

namespace minEngine
{
    class MaterialEditorViewportWindow final : public EditorViewportWindow
    {
    public:
        explicit MaterialEditorViewportWindow(IEditorContext& context)
            : EditorViewportWindow(context, "material_editor_preview", "Material Editor Viewport")
        {
            SetOpen(false);
        }

        std::string_view GetOwnerModuleId() const override { return MaterialEditor::kModuleId; }

        void OnAttach() override;

        MaterialEditorViewportClient& GetMaterialEditorViewportClient()
        {
            return static_cast<MaterialEditorViewportClient&>(GetViewportClient());
        }

        const MaterialEditorViewportClient& GetMaterialEditorViewportClient() const
        {
            return static_cast<const MaterialEditorViewportClient&>(GetViewportClient());
        }

    protected:
        EditorViewportClient& GetOrCreateViewportClient() override;
        const RHITextureRef& GetDisplayColorTexture() const override;
    };
}
