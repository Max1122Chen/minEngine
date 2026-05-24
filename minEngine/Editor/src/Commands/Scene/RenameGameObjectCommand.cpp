#include "Commands/Scene/RenameGameObjectCommand.h"

#include "Scene/SceneEditor.h"

namespace minEngine
{
    RenameGameObjectCommand::RenameGameObjectCommand(SceneEditor& sceneEditor,
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

    void RenameGameObjectCommand::Execute()
    {
        m_SceneEditor.ApplyRenameGameObject(m_GameObjectId, m_NewName);
    }

    void RenameGameObjectCommand::Undo()
    {
        m_SceneEditor.ApplyRenameGameObject(m_GameObjectId, m_OldName);
    }

    const char* RenameGameObjectCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}
