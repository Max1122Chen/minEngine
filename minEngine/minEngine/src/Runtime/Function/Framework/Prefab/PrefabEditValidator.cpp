#include "Runtime/Function/Framework/Prefab/PrefabEditValidator.h"

#include "Runtime/Function/Framework/Prefab/PrefabTypes.h"

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
            result.bAllowed = true;
            return result;

        case EPrefabEditOpKind::AddComponent:
            result.bAllowed = false;
            result.Error = "Adding components to Prefab instances is not supported in CORE-F24 MVP.";
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
            result.bAllowed = false;
            result.Error = "Removing components from Prefab instances is not supported in CORE-F24 MVP.";
            return result;
        }
        }

        result.bAllowed = false;
        result.Error = "Unknown Prefab edit operation.";
        return result;
    }
}
