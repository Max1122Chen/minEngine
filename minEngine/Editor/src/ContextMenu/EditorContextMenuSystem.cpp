#include "ContextMenu/EditorContextMenuSystem.h"

#include "ContextMenu/Actions/ContentBrowserBuiltInActions.h"
#include "ContextMenu/EditorActionRegistry.h"
#include "ContextMenu/EditorMenuBuilder.h"
#include "Shell/IEditorContext.h"

namespace minEngine
{
    EditorContextMenuSystem::EditorContextMenuSystem()
        : m_Builder(m_Registry)
    {
    }

    void EditorContextMenuSystem::RegisterBuiltInActions()
    {
        RegisterContentBrowserBuiltInActions(m_Registry);
    }

    void EditorContextMenuSystem::Shutdown()
    {
        m_Registry.ClearProviders();
    }

    void EditorContextMenuSystem::BuildAndDraw(IEditorContext& editor, const EditorMenuContext& ctx)
    {
        m_Builder.Draw(editor, ctx);
    }

    EditorActionRegistry& EditorContextMenuSystem::GetRegistry()
    {
        return m_Registry;
    }

    const EditorActionRegistry& EditorContextMenuSystem::GetRegistry() const
    {
        return m_Registry;
    }

} // namespace minEngine
