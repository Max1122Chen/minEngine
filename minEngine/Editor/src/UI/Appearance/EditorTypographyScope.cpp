#include "UI/Appearance/EditorTypographyScope.h"

#include "UI/Appearance/EditorAppearance.h"

#include "imgui.h"

namespace minEngine
{
    EditorTypographyScope::EditorTypographyScope(EditorAppearance& appearance, EditorTypographyRole role)
    {
        ImFont* font = appearance.GetImFont(role);
        if (font == nullptr)
        {
            return;
        }

        ImGui::PushFont(font, 0.0f);
        m_Pushed = true;
    }

    EditorTypographyScope::~EditorTypographyScope()
    {
        if (m_Pushed)
        {
            ImGui::PopFont();
        }
    }
}
