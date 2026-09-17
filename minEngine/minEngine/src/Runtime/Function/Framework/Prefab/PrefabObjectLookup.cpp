#include "Runtime/Function/Framework/Prefab/PrefabObjectLookup.h"

#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

namespace minEngine
{
    std::shared_ptr<MEObject> PrefabObjectLookup::FindSharedInScene(Scene& scene, const GUID& guid)
    {
        if (guid.IsZero())
        {
            return nullptr;
        }

        for (const std::shared_ptr<GameObject>& gameObject : scene.GetAllGameObjects())
        {
            if (!gameObject)
            {
                continue;
            }

            if (gameObject->GetGuid() == guid)
            {
                return gameObject;
            }

            for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
            {
                if (component && component->GetGuid() == guid)
                {
                    return component;
                }
            }
        }

        return nullptr;
    }

    MEObject* PrefabObjectLookup::FindInPrefab(Prefab& prefab, const GUID& guid)
    {
        return const_cast<MEObject*>(FindInPrefab(static_cast<const Prefab&>(prefab), guid));
    }

    const MEObject* PrefabObjectLookup::FindInPrefab(const Prefab& prefab, const GUID& guid)
    {
        if (guid.IsZero())
        {
            return nullptr;
        }

        for (const std::shared_ptr<GameObject>& gameObject : prefab.GetTemplateObjects())
        {
            if (!gameObject)
            {
                continue;
            }

            if (gameObject->GetGuid() == guid)
            {
                return gameObject.get();
            }

            for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
            {
                if (component && component->GetGuid() == guid)
                {
                    return component.get();
                }
            }
        }

        return nullptr;
    }

    bool PrefabObjectLookup::PrefabContains(const Prefab& prefab, const GUID& guid)
    {
        return FindInPrefab(prefab, guid) != nullptr;
    }
}
