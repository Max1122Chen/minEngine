#include "Commands/Scene/EditorAddComponentCommand.h"

#include "SubEditor/Scene/SceneEditor.h"

namespace minEngine
{
    EditorAddComponentCommand::EditorAddComponentCommand(SceneEditor& sceneEditor,
                                             uint64_t ownerGameObjectId,
                                             std::string componentTypeName)
        : m_SceneEditor(sceneEditor)
        , m_OwnerGameObjectId(ownerGameObjectId)
        , m_ComponentTypeName(std::move(componentTypeName))
    {
        m_Description = "Add Component";
    }

    void EditorAddComponentCommand::Execute()
    {
        m_SceneEditor.ApplyAddComponentToGameObject(
            m_OwnerGameObjectId, m_ComponentTypeName, m_CreatedComponent);
    }

    void EditorAddComponentCommand::Undo()
    {
        if (!m_CreatedComponent)
        {
            return;
        }

        if (m_SceneEditor.ApplyRemoveComponentFromGameObject(m_OwnerGameObjectId, *m_CreatedComponent))
        {
            m_CreatedComponent = nullptr;
        }
    }

    const char* EditorAddComponentCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}
