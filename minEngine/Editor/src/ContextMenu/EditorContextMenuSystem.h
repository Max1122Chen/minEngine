#pragma once

#include "Core.h"

#include "ContextMenu/EditorActionRegistry.h"
#include "ContextMenu/EditorMenuBuilder.h"

namespace minEngine
{
    class EditorMenuContext;
    class IEditorContext;

    class EditorContextMenuSystem
    {
    public:
        EditorContextMenuSystem();

        void RegisterBuiltInActions();
        void Shutdown();

        void BuildAndDraw(IEditorContext& editor, const EditorMenuContext& ctx);

        EditorActionRegistry& GetRegistry();
        const EditorActionRegistry& GetRegistry() const;

    private:
        EditorActionRegistry m_Registry;
        EditorMenuBuilder m_Builder;
    };

} // namespace minEngine
