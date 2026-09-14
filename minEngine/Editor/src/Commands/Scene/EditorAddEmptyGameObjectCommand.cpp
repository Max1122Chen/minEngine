#include "Commands/Scene/EditorAddEmptyGameObjectCommand.h"

#include "SubEditor/Scene/SceneEditor.h"

#include <limits>

namespace minEngine
{
    EditorAddEmptyGameObjectCommand::EditorAddEmptyGameObjectCommand(SceneEditor& sceneEditor)
        : m_SceneEditor(sceneEditor)
    {
        m_Description = "Create GameObject";
    }

    void EditorAddEmptyGameObjectCommand::Execute()
    {
        const uint64_t id = m_SceneEditor.ApplyAddEmptyGOToScene();
        if (id != std::numeric_limits<uint64_t>::max())
        {
            m_CreatedGameObjectId = id;
        }
    }

    void EditorAddEmptyGameObjectCommand::Undo()
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

    const char* EditorAddEmptyGameObjectCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

