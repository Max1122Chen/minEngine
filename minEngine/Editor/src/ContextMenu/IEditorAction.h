#pragma once

#include "ContextMenu/EditorActionIds.h"

namespace minEngine
{
    class EditorMenuContext;
    class IEditorContext;

    class IEditorAction
    {
    public:
        virtual ~IEditorAction() = default;

        virtual EditorActionId GetId() const = 0;
        virtual const char* GetLabel(const EditorMenuContext& ctx) const = 0;
        virtual EditorMenuSectionId GetSection() const = 0;
        virtual int GetSortOrder() const { return 0; }

        /** Whether the item appears in the menu (Scheme A: match Context, not executability). */
        virtual bool IsVisibleInMenu(const EditorMenuContext& ctx) const = 0;

        virtual bool CanExecute(const EditorMenuContext& ctx) const = 0;
        virtual const char* GetDisabledReason(const EditorMenuContext& ctx) const = 0;

        virtual void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const = 0;
    };

} // namespace minEngine
