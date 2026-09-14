#include "Commands/Scene/EditorReparentGameObjectCommand.h"

#include "SubEditor/Scene/SceneEditor.h"

namespace minEngine
{
    EditorReparentGameObjectCommand::EditorReparentGameObjectCommand(SceneEditor& sceneEditor,
                                                         uint64_t gameObjectId,
                                                         uint64_t oldParentId,
                                                         uint64_t newParentId)
        : m_SceneEditor(sceneEditor)
        , m_GameObjectId(gameObjectId)
        , m_OldParentId(oldParentId)
        , m_NewParentId(newParentId)
    {
        m_Description = "Reparent GameObject";
    }

    void EditorReparentGameObjectCommand::Execute()
    {
        m_SceneEditor.ApplyReparentGameObject(m_GameObjectId, m_NewParentId);
    }

    void EditorReparentGameObjectCommand::Undo()
    {
        m_SceneEditor.ApplyReparentGameObject(m_GameObjectId, m_OldParentId);
    }

    const char* EditorReparentGameObjectCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}