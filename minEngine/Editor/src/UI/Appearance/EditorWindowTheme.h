#pragma once

namespace minEngine
{
    class EditorAppearance;
    class EditorThemeScope;

    /** Thin helpers for common panel chrome (M6). Prefer EditorThemeScope for one-off roles. */
    class EditorWindowTheme
    {
    public:
        static EditorThemeScope SubduedSectionHeader(EditorAppearance& appearance);
        static EditorThemeScope SectionHeader(EditorAppearance& appearance);
        static EditorThemeScope Field(EditorAppearance& appearance);
        static EditorThemeScope HierarchySelection(EditorAppearance& appearance);
        static EditorThemeScope PanelOverlay(EditorAppearance& appearance);
        static EditorThemeScope PrimaryText(EditorAppearance& appearance);
    };
}
