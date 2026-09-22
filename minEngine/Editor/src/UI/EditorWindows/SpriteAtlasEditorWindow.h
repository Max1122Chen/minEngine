#pragma once

#include "UI/EditorWindows/EditorWindow.h"

#include "Platform/EditorRHIImGuiTexture.h"

#include <string>

namespace minEngine
{
    class SpriteAtlasEditorWindow : public EditorWindow
    {
    public:
        explicit SpriteAtlasEditorWindow(IEditorContext& context);

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }
        std::string_view GetOwnerModuleId() const override;

        void OnDraw() override;

    private:
        void DrawMembersAndControls();
        void DrawGeneratedPreview();

        std::string m_Id = "sprite_atlas_editor";
        std::string m_Title = "Sprite Atlas Editor";
        EditorRHIImGuiTexturePin m_PinnedGeneratedTexture;
        int m_AddSpritePickerIndex = -1;
    };
}
