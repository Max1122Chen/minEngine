#include "Commands/Scene/SetGameObjectTransformCommand.h"

#include "Scene/SceneEditor.h"

namespace minEngine
{
    SetGameObjectTransformCommand::SetGameObjectTransformCommand(SceneEditor& sceneEditor,
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

    void SetGameObjectTransformCommand::Execute()
    {
        m_SceneEditor.ApplyGameObjectTransform(m_GameObjectId, m_After);
    }

    void SetGameObjectTransformCommand::Undo()
    {
        m_SceneEditor.ApplyGameObjectTransform(m_GameObjectId, m_Before);
    }

    const char* SetGameObjectTransformCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}
