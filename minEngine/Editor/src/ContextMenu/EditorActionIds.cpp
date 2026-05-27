#include "ContextMenu/EditorActionIds.h"

namespace minEngine
{
    const char* GetEditorMenuSectionDisplayName(EditorMenuSectionId section)
    {
        switch (section)
        {
        case EditorMenuSectionId::Edit:
            return "Edit";
        case EditorMenuSectionId::Asset:
            return "Asset";
        case EditorMenuSectionId::Create:
            return "Create";
        case EditorMenuSectionId::View:
            return "View";
        default:
            return "";
        }
    }

    int GetEditorMenuSectionSortOrder(EditorMenuSectionId section)
    {
        switch (section)
        {
        case EditorMenuSectionId::Edit:
            return 0;
        case EditorMenuSectionId::Asset:
            return 1;
        case EditorMenuSectionId::Create:
            return 2;
        case EditorMenuSectionId::View:
            return 3;
        default:
            return 99;
        }
    }

} // namespace minEngine
