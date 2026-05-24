#include "Commands/Scene/RemoveComponentCommand.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Scene/SceneEditor.h"

namespace minEngine
{
    RemoveComponentCommand::RemoveComponentCommand(SceneEditor& sceneEditor,
                                                   uint64_t ownerGameObjectId,
                                                   std::string componentTypeName)
        : m_SceneEditor(sceneEditor)
        , m_OwnerGameObjectId(ownerGameObjectId)
        , m_ComponentTypeName(std::move(componentTypeName))
    {
        m_Description = "Remove Component";
    }

    bool RemoveComponentCommand::TryFindFirstComponentByType(GameObject& owner, Component*& outComponent) const
    {
        outComponent = nullptr;
        for (const std::shared_ptr<Component>& componentPtr : owner.GetAllComponents())
        {
            if (!componentPtr)
            {
                continue;
            }

            const Reflection::MEClass* classInfo = componentPtr->GetClass();
            if (!classInfo)
            {
                continue;
            }

            if (classInfo->GetName() == m_ComponentTypeName)
            {
                outComponent = componentPtr.get();
                return true;
            }
        }
        return false;
    }

    void RemoveComponentCommand::Execute()
    {
        m_Removed = false;

        m_SceneEditor.SelectGameObject(m_OwnerGameObjectId);
        GameObject* owner = m_SceneEditor.GetSelectedGameObject();
        if (!owner)
        {
            return;
        }

        Component* component = nullptr;
        if (!TryFindFirstComponentByType(*owner, component) || !component)
        {
            return;
        }

        m_Removed = m_SceneEditor.ApplyRemoveComponentFromGO(*owner, *component);
    }

    void RemoveComponentCommand::Undo()
    {
        if (!m_Removed)
        {
            return;
        }

        // Best-effort restore: re-adds a fresh component of the same type (state is not preserved yet).
        m_SceneEditor.SelectGameObject(m_OwnerGameObjectId);
        Component* newComponent = nullptr;
        if (m_SceneEditor.ApplyAddComponentToSelectedGameObject(m_ComponentTypeName, newComponent))
        {
            m_Removed = false;
        }
    }

    const char* RemoveComponentCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

