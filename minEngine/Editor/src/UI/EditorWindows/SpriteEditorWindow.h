#pragma once

#include "UI/EditorWindows/EditorWindow.h"

#include "Platform/EditorRHIImGuiTexture.h"
#include "Runtime/Core/Math/Math.h"

#include <string>

namespace minEngine
{
    class SpriteEditorWindow : public EditorWindow
    {
    public:
        explicit SpriteEditorWindow(IEditorContext& context);

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }
        std::string_view GetOwnerModuleId() const override;

        void OnDraw() override;

    private:
        void DrawPreviewAndProperties();

        std::string m_Id = "sprite_editor";
        std::string m_Title = "Sprite Editor";
        EditorRHIImGuiTexturePin m_PinnedSourceTexture;
        bool m_IsDraggingRect = false;
        ImVec2 m_DragStartImagePx{ 0.0f, 0.0f };
        Vector4 m_DragRectPx{ 0.0f, 0.0f, 0.0f, 0.0f };
    };
}
