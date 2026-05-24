#include "Commands/Scene/AddComponentCommand.h"

#include "Scene/SceneEditor.h"

namespace minEngine
{
    AddComponentCommand::AddComponentCommand(SceneEditor& sceneEditor,
                                             uint64_t ownerGameObjectId,
                                             std::string componentTypeName)
        : m_SceneEditor(sceneEditor)
        , m_OwnerGameObjectId(ownerGameObjectId)
        , m_ComponentTypeName(std::move(componentTypeName))
    {
        m_Description = "Add Component";
    }

    void AddComponentCommand::Execute()
    {
        // Ensure the desired owner is selected before applying.
        m_SceneEditor.SelectGameObject(m_OwnerGameObjectId);
        m_SceneEditor.ApplyAddComponentToSelectedGameObject(m_ComponentTypeName, m_CreatedComponent);
    }

    void AddComponentCommand::Undo()
    {
        if (!m_CreatedComponent)
        {
            return;
        }

        // Owner might have changed selection; force selection for safety.
        m_SceneEditor.SelectGameObject(m_OwnerGameObjectId);
        GameObject* owner = m_SceneEditor.GetSelectedGameObject();
        if (!owner)
        {
            return;
        }

        if (m_SceneEditor.ApplyRemoveComponentFromGO(*owner, *m_CreatedComponent))
        {
            m_CreatedComponent = nullptr;
        }
    }

    const char* AddComponentCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

