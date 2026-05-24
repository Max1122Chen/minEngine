#include "Commands/Scene/DeleteGameObjectCommand.h"

#include "Scene/SceneEditor.h"

#include <limits>

namespace minEngine
{
    DeleteGameObjectCommand::DeleteGameObjectCommand(SceneEditor& sceneEditor, uint64_t gameObjectId)
        : m_SceneEditor(sceneEditor)
        , m_GameObjectId(gameObjectId)
    {
        m_Description = "Delete GameObject";
    }

    void DeleteGameObjectCommand::Execute()
    {
        m_Name.clear();
        m_Transform = Transform{};
        m_HasSnapshot = m_SceneEditor.ApplyRemoveGameObjectFromScene(m_GameObjectId, m_Name, m_Transform);
    }

    void DeleteGameObjectCommand::Undo()
    {
        if (!m_HasSnapshot)
        {
            return;
        }

        const uint64_t restoredId = m_SceneEditor.ApplyRestoreRemovedGameObject(m_Name, m_Transform);
        if (restoredId != std::numeric_limits<uint64_t>::max())
        {
            m_GameObjectId = restoredId;
        }
    }

    const char* DeleteGameObjectCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

