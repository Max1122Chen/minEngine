#include "ContextMenu/EditorContextMenuSystem.h"

#include "ContextMenu/Actions/ContentBrowserBuiltInActions.h"
#include "ContextMenu/Actions/EditorEditActions.h"
#include "ContextMenu/Actions/SceneBuiltInActions.h"
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
        RegisterEditorEditActions(m_Registry);
        RegisterSceneBuiltInActions(m_Registry);
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
