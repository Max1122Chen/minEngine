#pragma once

#include "ContextMenu/EditorActionIds.h"

#include <functional>

namespace minEngine
{
    class EditorActionRegistry;
    class EditorMenuContext;
    class IEditorAction;
    class IEditorContext;

    class EditorMenuBuilder
    {
    public:
        explicit EditorMenuBuilder(EditorActionRegistry& registry);

        void Draw(IEditorContext& editor, const EditorMenuContext& ctx);

        /** Opens a section submenu if not already open (for Provider use). */
        bool EnsureSectionOpen(EditorMenuSectionId section);

        void DrawProviderMenuItem(
            IEditorContext& editor,
            const EditorMenuContext& ctx,
            const char* label,
            bool enabled,
            const char* disabledReason,
            const std::function<void(IEditorContext& editor, const EditorMenuContext& menuCtx)>& onExecute);

    private:
        void DrawStaticActions(IEditorContext& editor, const EditorMenuContext& ctx);

        void DrawAction(
            IEditorContext& editor,
            const IEditorAction& action,
            const EditorMenuContext& ctx,
            bool enabled);

        EditorActionRegistry& m_Registry;
        bool m_IsProviderCreateMenuOpen = false;
    };

} // namespace minEngine
