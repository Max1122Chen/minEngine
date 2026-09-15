#include "Commands/Scene/EditorRenameGameObjectCommand.h"

#include "SubEditor/Scene/SceneEditor.h"

namespace minEngine
{
    EditorRenameGameObjectCommand::EditorRenameGameObjectCommand(SceneEditor& sceneEditor,
                                                     uint64_t gameObjectId,
                                                     std::string oldName,
                                                     std::string newName)
        : m_SceneEditor(sceneEditor)
        , m_GameObjectId(gameObjectId)
        , m_OldName(std::move(oldName))
        , m_NewName(std::move(newName))
    {
        m_Description = "Rename GameObject";
    }

    void EditorRenameGameObjectCommand::Execute()
    {
        m_SceneEditor.ApplyRenameGameObject(m_GameObjectId, m_NewName);
    }

    void EditorRenameGameObjectCommand::Undo()
    {
        m_SceneEditor.ApplyRenameGameObject(m_GameObjectId, m_OldName);
    }

    const char* EditorRenameGameObjectCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}
