#pragma once

#include "UI/Appearance/EditorThemeColorRole.h"

namespace minEngine
{
    class EditorAppearance;

    class EditorThemeScope
    {
    public:
        EditorThemeScope(EditorAppearance& appearance,
                         EditorThemeColorRole role,
                         float alphaScale = 1.0f);
        ~EditorThemeScope();

        EditorThemeScope(EditorThemeScope&& other) noexcept;
        EditorThemeScope& operator=(EditorThemeScope&& other) noexcept;

        EditorThemeScope(const EditorThemeScope&) = delete;
        EditorThemeScope& operator=(const EditorThemeScope&) = delete;

    private:
        int m_PushedCount = 0;
    };
}
