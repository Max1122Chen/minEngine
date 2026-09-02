#include "PlayObjectMapping.h"

#include "Runtime/Function/Framework/Scene/SceneCloneContext.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    void PlayObjectMapping::Build(const SceneCloneContext& cloneContext)
    {
        Clear();
        for (const auto& [editorGuid, pieGuid] : cloneContext.SourceToClonedGuid)
        {
            if (editorGuid.IsZero() || pieGuid.IsZero())
            {
                continue;
            }

            m_EditorToPIE[editorGuid] = pieGuid;
            m_PIEToEditor[pieGuid] = editorGuid;
        }
    }

    void PlayObjectMapping::Clear()
    {
        m_EditorToPIE.clear();
        m_PIEToEditor.clear();
    }

    GameObject* PlayObjectMapping::FindPIECounterpart(const GameObject& editorGO) const
    {
        const auto iter = m_EditorToPIE.find(editorGO.GetGuid());
        if (iter == m_EditorToPIE.end())
        {
            return nullptr;
        }

        const std::shared_ptr<MEObject> pieObject = ObjectManager::Get().FindObject(iter->second);
        return dynamic_cast<GameObject*>(pieObject.get());
    }

    GameObject* PlayObjectMapping::FindEditorCounterpart(const GameObject& pieGO) const
    {
        const auto iter = m_PIEToEditor.find(pieGO.GetGuid());
        if (iter == m_PIEToEditor.end())
        {
            return nullptr;
        }

        const std::shared_ptr<MEObject> editorObject = ObjectManager::Get().FindObject(iter->second);
        return dynamic_cast<GameObject*>(editorObject.get());
    }

    Component* PlayObjectMapping::FindPIECounterpart(const Component& editorComp) const
    {
        const auto iter = m_EditorToPIE.find(editorComp.GetGuid());
        if (iter == m_EditorToPIE.end())
        {
            return nullptr;
        }

        const std::shared_ptr<MEObject> pieObject = ObjectManager::Get().FindObject(iter->second);
        return dynamic_cast<Component*>(pieObject.get());
    }
}
