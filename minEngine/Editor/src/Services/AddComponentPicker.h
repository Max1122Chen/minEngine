#pragma once

#include "Shell/IEditorContext.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    class SceneEditor;

    /**
     * Shared Add Component list drawing for Inspector panel and context submenu.
     * Same type source / icons / TypeDisplay / Submit path; different layouts.
     */
    class AddComponentPicker
    {
    public:
        /**
         * Inspector: full-width button opens searchable dropdown popup with scroll list.
         * Clicking a type adds immediately and closes the popup.
         */
        static bool DrawInspectorAddSection(IEditorContext& editor,
                                            SceneEditor& sceneEditor,
                                            std::string& selectedTypeNameInOut,
                                            char* filterBuffer,
                                            size_t filterBufferSize);

        /**
         * Draw contents inside an already-opened BeginMenu("Add Component").
         * Search + scrollable list. On type click: SubmitAdd with explicit GO id.
         */
        static void DrawContextSubMenu(IEditorContext& editor,
                                       SceneEditor& sceneEditor,
                                       uint64_t targetGameObjectId,
                                       char* filterBuffer,
                                       size_t filterBufferSize);

    private:
        static bool MatchesFilter(std::string_view typeName, std::string_view filterText);
        static void DrawTypeRowIcon(IEditorContext& editor, std::string_view typeName);
        static bool DrawFilteredTypeList(IEditorContext& editor,
                                         SceneEditor& sceneEditor,
                                         const std::vector<std::string>& componentTypeNames,
                                         std::string_view filterText,
                                         uint64_t targetGameObjectId,
                                         bool closePopupOnAdd);
    };
}
