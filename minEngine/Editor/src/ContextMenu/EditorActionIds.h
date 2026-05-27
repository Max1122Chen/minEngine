#pragma once

#include "Core.h"

#include <cstdint>

namespace minEngine
{
    enum class EditorActionId : uint32_t
    {
        None = 0,
        Delete,
        Rename,
        Duplicate,
        RevealInExplorer,
        ImportAsset,
        Refresh,
        FocusInViewport,
        OpenAsset,
        CreateEmptyGameObject,
        RemoveComponent,
    };

    enum class EditorMenuSectionId : uint32_t
    {
        Edit,
        Asset,
        Create,
        View,
    };

    const char* GetEditorMenuSectionDisplayName(EditorMenuSectionId section);
    int GetEditorMenuSectionSortOrder(EditorMenuSectionId section);

} // namespace minEngine
