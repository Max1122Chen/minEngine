#include "Commands/Scene/EditorSetGameObjectTransformCommand.h"

#include "SubEditor/Scene/SceneEditor.h"

namespace minEngine
{
    EditorSetGameObjectTransformCommand::EditorSetGameObjectTransformCommand(SceneEditor& sceneEditor,
                                                                 uint64_t gameObjectId,
                                                                 Transform before,
                                                                 Transform after)
        : m_SceneEditor(sceneEditor)
        , m_GameObjectId(gameObjectId)
        , m_Before(before)
        , m_After(after)
    {
        m_Description = "Transform GameObject";
    }

    void EditorSetGameObjectTransformCommand::Execute()
    {
        m_SceneEditor.ApplyGameObjectTransform(m_GameObjectId, m_After);
    }

    void EditorSetGameObjectTransformCommand::Undo()
    {
        m_SceneEditor.ApplyGameObjectTransform(m_GameObjectId, m_Before);
    }

    const char* EditorSetGameObjectTransformCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}
