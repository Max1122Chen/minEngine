#include "Commands/Scene/AddEmptyGameObjectCommand.h"

#include "SubEditor/Scene/SceneEditor.h"

#include <limits>

namespace minEngine
{
    AddEmptyGameObjectCommand::AddEmptyGameObjectCommand(SceneEditor& sceneEditor)
        : m_SceneEditor(sceneEditor)
    {
        m_Description = "Create GameObject";
    }

    void AddEmptyGameObjectCommand::Execute()
    {
        const uint64_t id = m_SceneEditor.ApplyAddEmptyGOToScene();
        if (id != std::numeric_limits<uint64_t>::max())
        {
            m_CreatedGameObjectId = id;
        }
    }

    void AddEmptyGameObjectCommand::Undo()
    {
        if (m_CreatedGameObjectId == std::numeric_limits<uint64_t>::max())
        {
            return;
        }

        std::string name;
        Transform transform;
        if (m_SceneEditor.ApplyRemoveGameObjectFromScene(m_CreatedGameObjectId, name, transform))
        {
            // Command remains valid; future redo will create a new GameObject and update id.
            m_CreatedGameObjectId = std::numeric_limits<uint64_t>::max();
        }
    }

    const char* AddEmptyGameObjectCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

