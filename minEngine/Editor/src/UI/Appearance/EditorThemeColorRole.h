#pragma once

namespace minEngine
{
    /** Maps editor chrome to palette tokens or fixed semantic colors (see EditorAppearance). */
    enum class EditorThemeColorRole
    {
        Field,
        SectionHeader,
        SubduedSectionHeader,
        PanelOverlay,
        PrimaryText,
        /** Hierarchy list row selection — fixed blue accent (not palette Accent). */
        HierarchySelection,
    };
}
