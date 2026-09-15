#include "Runtime/Function/Framework/Prefab/PrefabEditValidator.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

namespace minEngine
{
    PrefabEditValidationResult PrefabEditValidator::ValidateEdit(
        Scene& /*scene*/,
        const PrefabInstanceRecord& record,
        const PrefabEditOp& op)
    {
        PrefabEditValidationResult result;

        switch (op.Kind)
        {
        case EPrefabEditOpKind::PropertyEdit:
        case EPrefabEditOpKind::AddComponent:
            result.bAllowed = true;
            return result;

        case EPrefabEditOpKind::AddTopLevelGameObject:
            result.bAllowed = false;
            result.Error = "Prefab instances cannot gain a second top-level GameObject.";
            return result;

        case EPrefabEditOpKind::DeleteGameObject:
            if (op.TargetInstanceGuid == record.RootInstanceGuid)
            {
                result.bAllowed = false;
                result.Error = "Cannot delete Prefab instance root.";
                return result;
            }
            result.bAllowed = false;
            result.Error = "Deleting mapped Prefab nodes is not supported in CORE-F24 MVP.";
            return result;

        case EPrefabEditOpKind::Reparent:
        {
            if (op.TargetInstanceGuid == record.RootInstanceGuid)
            {
                // Allow reparenting the whole instance root under a non-prefab parent (scene placement).
                result.bAllowed = true;
                return result;
            }

            bool newParentInInstance = false;
            for (const PrefabObjectMapping& mapping : record.ObjectMappings)
            {
                if (mapping.InstanceGuid == op.NewParentInstanceGuid)
                {
                    newParentInInstance = true;
                    break;
                }
            }

            if (!newParentInInstance && !op.NewParentInstanceGuid.IsZero())
            {
                result.bAllowed = false;
                result.Error = "Cannot reparent Prefab child outside its instance tree.";
                return result;
            }

            result.bAllowed = false;
            result.Error = "Reparenting within a Prefab instance is restricted in CORE-F24 MVP.";
            return result;
        }

        case EPrefabEditOpKind::RemoveComponent:
        {
            std::shared_ptr<MEObject> object = ObjectManager::HasInstance()
                ? ObjectManager::Get().FindObject(op.TargetInstanceGuid)
                : nullptr;
            Component* component = dynamic_cast<Component*>(object.get());
            if (component == nullptr)
            {
                result.bAllowed = false;
                result.Error = "RemoveComponent target is not a Component.";
                return result;
            }

            GameObject* owner = dynamic_cast<GameObject*>(const_cast<MEObject*>(component->GetOuter()));
            if (owner != nullptr && owner->GetRootComponent() == component)
            {
                result.bAllowed = false;
                result.Error = "Cannot remove Root SceneComponent from a Prefab instance.";
                return result;
            }

            result.bAllowed = true;
            return result;
        }
        }

        result.bAllowed = false;
        result.Error = "Unknown Prefab edit operation.";
        return result;
    }
}
