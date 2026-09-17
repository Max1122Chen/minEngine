#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"

#include "Runtime/Core/Object/ObjectCloneContext.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Function/Framework/Prefab/PrefabObjectLookup.h"
#include "Runtime/Function/Framework/Prefab/PrefabUtilityDetail.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

#include <unordered_map>

namespace minEngine
{
    bool PrefabUtility::WriteStageTreeToPrefab(
        Scene& stageScene,
        Prefab& prefab,
        ObjectCloneContext& inOutEditMap,
        std::string* outError)
    {
        // Fail-closed: do not rebuild EditCloneMap by BFS index (topology-fragile).
        auto rootMapIter = inOutEditMap.SourceToClonedGuid.find(prefab.GetRootGuid());
        if (rootMapIter == inOutEditMap.SourceToClonedGuid.end())
        {
            if (outError)
            {
                *outError =
                    "EditCloneMap missing Prefab root mapping; reopen the Prefab Stage and try Save again.";
            }
            return false;
        }

        const GUID stageRootGuid = rootMapIter->second;
        std::shared_ptr<GameObject> stageRoot = std::dynamic_pointer_cast<GameObject>(
            PrefabObjectLookup::FindSharedInScene(stageScene, stageRootGuid));
        if (!stageRoot)
        {
            if (outError)
            {
                *outError = "Stage root GameObject not found for Prefab Save.";
            }
            return false;
        }

        if (stageRoot->GetParent() != nullptr)
        {
            if (outError)
            {
                *outError = "Stage root must be unparented.";
            }
            return false;
        }

        size_t topLevelContentCount = 0;
        for (const std::shared_ptr<GameObject>& gameObject : stageScene.GetAllGameObjects())
        {
            if (!gameObject || gameObject->GetParent() != nullptr)
            {
                continue;
            }

            if (IsEditorTempStageObject(*gameObject))
            {
                continue;
            }

            ++topLevelContentCount;
        }
        if (topLevelContentCount != 1)
        {
            if (outError)
            {
                *outError = "Prefab Stage must have exactly one top-level GameObject.";
            }
            return false;
        }

        std::unordered_map<GUID, GUID, GUID::Hash> stageToTemplateGuid;
        for (const auto& pair : inOutEditMap.SourceToClonedGuid)
        {
            stageToTemplateGuid[pair.second] = pair.first;
        }

        std::vector<GameObject*> subtree;
        PrefabUtilityDetail::CollectSubtree(*stageRoot, subtree);

        ObjectCloneContext writeContext;
        std::vector<Serialization::PendingObjectRef> unresolvedRefs;
        std::vector<std::shared_ptr<GameObject>> newTemplates;
        newTemplates.reserve(subtree.size());

        for (GameObject* stageObject : subtree)
        {
            if (stageObject == nullptr)
            {
                continue;
            }

            std::string cloneError;
            std::shared_ptr<GameObject> cloned = PrefabUtilityDetail::CloneGameObjectAsTemplate(
                *stageObject, writeContext, unresolvedRefs, &cloneError);
            if (!cloned)
            {
                if (outError)
                {
                    *outError = cloneError.empty() ? "Clone Stage GameObject failed." : cloneError;
                }
                return false;
            }

            newTemplates.push_back(cloned);
        }

        const std::vector<std::shared_ptr<GameObject>> previousTemplates = prefab.GetTemplateObjects();
        const GUID previousRootGuid = prefab.GetRootGuid();

        prefab.ClearTemplateObjects();

        for (auto& pair : writeContext.ClonedBySourceGuid)
        {
            const GUID& stageGuid = pair.first;
            std::shared_ptr<MEObject>& clonedObject = pair.second;
            if (!clonedObject)
            {
                continue;
            }

            GUID desiredGuid = writeContext.SourceToClonedGuid[stageGuid];
            const auto mapped = stageToTemplateGuid.find(stageGuid);
            if (mapped != stageToTemplateGuid.end() && !mapped->second.IsZero())
            {
                desiredGuid = mapped->second;
            }

            if (clonedObject->GetGuid() != desiredGuid)
            {
                ObjectManager::Get().RemapObjectGuid(clonedObject, desiredGuid);
            }
            writeContext.SourceToClonedGuid[stageGuid] = desiredGuid;
        }

        Serialization::Serializer::SetActiveCloneContext(&writeContext);
        const Serialization::SerializeResult resolveResult =
            Serialization::Serializer::ResolvePendingObjectRefs(unresolvedRefs);
        Serialization::Serializer::SetActiveCloneContext(nullptr);
        if (!resolveResult.ok)
        {
            PrefabUtilityDetail::RestorePreviousTemplates(prefab, previousTemplates, previousRootGuid);
            if (outError)
            {
                *outError = resolveResult.message;
            }
            return false;
        }

        const GUID newRootGuid = writeContext.SourceToClonedGuid[stageRootGuid];
        if (newRootGuid.IsZero())
        {
            PrefabUtilityDetail::RestorePreviousTemplates(prefab, previousTemplates, previousRootGuid);
            if (outError)
            {
                *outError = "Failed to resolve Prefab root Guid after Stage writeback.";
            }
            return false;
        }

        for (const std::shared_ptr<GameObject>& templateObject : newTemplates)
        {
            if (!templateObject)
            {
                continue;
            }

            templateObject->SetOuter(&prefab);
            if (templateObject->GetGuid() == newRootGuid)
            {
                templateObject->DetachFromParent(AttachmentTransformRules::KeepWorldTransform);
            }
            prefab.AddTemplateObject(templateObject);
        }

        prefab.SetRootGuid(newRootGuid);

        std::string validateError;
        if (!prefab.ValidateSingleRoot(&validateError))
        {
            PrefabUtilityDetail::RestorePreviousTemplates(prefab, previousTemplates, previousRootGuid);
            if (outError)
            {
                *outError = validateError;
            }
            return false;
        }

        PrefabUtilityDetail::RefreshEditCloneMapAfterWrite(stageScene, writeContext, inOutEditMap);

        if (inOutEditMap.SourceToClonedGuid.find(prefab.GetRootGuid()) == inOutEditMap.SourceToClonedGuid.end())
        {
            PrefabUtilityDetail::RestorePreviousTemplates(prefab, previousTemplates, previousRootGuid);
            if (outError)
            {
                *outError =
                    "EditCloneMap lost Prefab root mapping after Save refresh; Stage templates restored.";
            }
            return false;
        }

        return true;
    }
}
