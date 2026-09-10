#include "Commands/Scene/ReparentGameObjectCommand.h"

#include "SubEditor/Scene/SceneEditor.h"

namespace minEngine
{
    ReparentGameObjectCommand::ReparentGameObjectCommand(SceneEditor& sceneEditor,
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

    void ReparentGameObjectCommand::Execute()
    {
        m_SceneEditor.ApplyReparentGameObject(m_GameObjectId, m_NewParentId);
    }

    void ReparentGameObjectCommand::Undo()
    {
        m_SceneEditor.ApplyReparentGameObject(m_GameObjectId, m_OldParentId);
    }

    const char* ReparentGameObjectCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}