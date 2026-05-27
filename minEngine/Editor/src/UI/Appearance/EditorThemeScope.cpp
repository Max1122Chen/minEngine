#include "UI/Appearance/EditorThemeScope.h"

#include "UI/Appearance/EditorAppearance.h"

#include "imgui.h"

namespace minEngine
{
    EditorThemeScope::EditorThemeScope(EditorAppearance& appearance,
                                       EditorThemeColorRole role,
                                       float alphaScale)
    {
        m_PushedCount = appearance.PushThemeColors(role, alphaScale);
    }

    EditorThemeScope::EditorThemeScope(EditorThemeScope&& other) noexcept
        : m_PushedCount(other.m_PushedCount)
    {
        other.m_PushedCount = 0;
    }

    EditorThemeScope& EditorThemeScope::operator=(EditorThemeScope&& other) noexcept
    {
        if (this != &other)
        {
            if (m_PushedCount > 0)
            {
                ImGui::PopStyleColor(m_PushedCount);
                m_PushedCount = 0;
            }

            m_PushedCount = other.m_PushedCount;
            other.m_PushedCount = 0;
        }

        return *this;
    }

    EditorThemeScope::~EditorThemeScope()
    {
        if (m_PushedCount > 0)
        {
            ImGui::PopStyleColor(m_PushedCount);
            m_PushedCount = 0;
        }
    }
}
