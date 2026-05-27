#include "UI/Appearance/EditorWindowTheme.h"

#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorThemeScope.h"

namespace minEngine
{
    EditorThemeScope EditorWindowTheme::SubduedSectionHeader(EditorAppearance& appearance)
    {
        return EditorThemeScope(appearance, EditorThemeColorRole::SubduedSectionHeader);
    }

    EditorThemeScope EditorWindowTheme::SectionHeader(EditorAppearance& appearance)
    {
        return EditorThemeScope(appearance, EditorThemeColorRole::SectionHeader);
    }

    EditorThemeScope EditorWindowTheme::Field(EditorAppearance& appearance)
    {
        return EditorThemeScope(appearance, EditorThemeColorRole::Field);
    }

    EditorThemeScope EditorWindowTheme::HierarchySelection(EditorAppearance& appearance)
    {
        return EditorThemeScope(appearance, EditorThemeColorRole::HierarchySelection);
    }

    EditorThemeScope EditorWindowTheme::PanelOverlay(EditorAppearance& appearance)
    {
        return EditorThemeScope(appearance, EditorThemeColorRole::PanelOverlay, 0.82f);
    }

    EditorThemeScope EditorWindowTheme::PrimaryText(EditorAppearance& appearance)
    {
        return EditorThemeScope(appearance, EditorThemeColorRole::PrimaryText);
    }
}
