#include "Commands/Scene/EditorInstantiatePrefabCommand.h"

#include "SubEditor/Scene/SceneEditor.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"

#include <limits>

namespace minEngine
{
    EditorInstantiatePrefabCommand::EditorInstantiatePrefabCommand(
        SceneEditor& sceneEditor,
        std::shared_ptr<Prefab> prefab,
        GameObject* attachParent)
        : m_SceneEditor(sceneEditor)
        , m_Prefab(std::move(prefab))
        , m_AttachParentId(
              attachParent != nullptr ? attachParent->GetID() : std::numeric_limits<uint64_t>::max())
    {
        m_Description = "Instantiate Prefab";
    }

    void EditorInstantiatePrefabCommand::Execute()
    {
        if (!m_Prefab)
        {
            return;
        }

        const uint64_t id = m_SceneEditor.ApplyInstantiatePrefab(*m_Prefab, m_AttachParentId);
        if (id != std::numeric_limits<uint64_t>::max())
        {
            m_CreatedRootId = id;
        }
    }

    void EditorInstantiatePrefabCommand::Undo()
    {
        if (m_CreatedRootId == 0 || m_CreatedRootId == std::numeric_limits<uint64_t>::max())
        {
            return;
        }

        std::string name;
        Transform transform;
        if (m_SceneEditor.ApplyRemoveGameObjectFromScene(m_CreatedRootId, name, transform))
        {
            m_CreatedRootId = std::numeric_limits<uint64_t>::max();
        }
    }

    const char* EditorInstantiatePrefabCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}
