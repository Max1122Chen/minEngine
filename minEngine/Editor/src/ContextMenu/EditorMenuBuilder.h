#pragma once

#include "ContextMenu/EditorActionIds.h"

#include <vector>

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

    private:
        void DrawAction(
            IEditorContext& editor,
            const IEditorAction& action,
            const EditorMenuContext& ctx,
            bool enabled);

        EditorActionRegistry& m_Registry;
    };

} // namespace minEngine
