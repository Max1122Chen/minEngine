#include "Runtime/Function/Framework/Prefab/PrefabUtilityDetail.h"

#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Function/Framework/Prefab/PrefabObjectLookup.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

namespace minEngine
{
    void PrefabUtilityDetail::CollectSubtree(GameObject& root, std::vector<GameObject*>& outOrdered)
    {
        outOrdered.clear();
        outOrdered.push_back(&root);

        size_t index = 0;
        while (index < outOrdered.size())
        {
            GameObject* current = outOrdered[index++];
            for (GameObject* child : current->GetChildren())
            {
                if (child != nullptr)
                {
                    outOrdered.push_back(child);
                }
            }
        }
    }

    std::shared_ptr<GameObject> PrefabUtilityDetail::CloneGameObjectAsTemplate(
        GameObject& source,
        ObjectCloneContext& cloneContext,
        std::vector<Serialization::PendingObjectRef>& unresolvedRefs,
        std::string* outError)
    {
        const GUID sourceGuid = source.GetGuid();

        std::vector<uint8_t> buffer;
        const Serialization::SerializeResult serializeResult =
            Serialization::Serializer::SerializeObjectToBuffer(&source, buffer, kPrefabCloneOptions);
        if (!serializeResult.ok)
        {
            if (outError)
            {
                *outError = serializeResult.message;
            }
            return nullptr;
        }

        std::shared_ptr<GameObject> cloned = NewObject<GameObject>();
        if (ObjectManager::HasInstance())
        {
            ObjectManager::Get().UnregisterObject(cloned.get());
        }

        Serialization::Serializer::SetActiveCloneContext(&cloneContext);
        const Serialization::SerializeResult deserializeResult =
            Serialization::Serializer::DeserializeObjectFromBuffer(
                cloned.get(),
                buffer,
                unresolvedRefs,
                kPrefabCloneOptions);
        Serialization::Serializer::SetActiveCloneContext(nullptr);

        if (!deserializeResult.ok)
        {
            if (outError)
            {
                *outError = deserializeResult.message;
            }
            return nullptr;
        }

        const GUID clonedGuid = GenerateGUID();
        cloneContext.RecordClone(sourceGuid, cloned, clonedGuid);
        ObjectManager::Get().RemapObjectGuid(cloned, clonedGuid);

        return cloned;
    }

    void PrefabUtilityDetail::BuildInstanceRecordFromCloneContext(
        PrefabInstanceRecord& record,
        const GUID& prefabAssetGuid,
        const GUID& rootInstanceGuid,
        const ObjectCloneContext& cloneContext,
        EMappingDirection direction)
    {
        record.PrefabAssetGuid = prefabAssetGuid;
        record.RootInstanceGuid = rootInstanceGuid;
        record.ObjectMappings.clear();
        record.Overrides.clear();

        for (const auto& pair : cloneContext.SourceToClonedGuid)
        {
            PrefabObjectMapping mapping;
            if (direction == EMappingDirection::TemplateToInstance)
            {
                mapping.TemplateGuid = pair.first;
                mapping.InstanceGuid = pair.second;
            }
            else
            {
                mapping.TemplateGuid = pair.second;
                mapping.InstanceGuid = pair.first;
            }
            record.ObjectMappings.push_back(mapping);
        }
    }

    void PrefabUtilityDetail::ClearUnresolvedPendingRefs(
        std::vector<Serialization::PendingObjectRef>& unresolvedRefs,
        PrefabCreateReport* outReport)
    {
        for (const Serialization::PendingObjectRef& pendingRef : unresolvedRefs)
        {
            if (outReport != nullptr)
            {
                PrefabBrokenRef broken;
                if (pendingRef.ownerObjectPtr != nullptr)
                {
                    const MEObject* owner = static_cast<const MEObject*>(pendingRef.ownerObjectPtr);
                    broken.OwnerTemplateGuid = owner->GetGuid();
                }
                broken.PropertyPath = pendingRef.fieldPath;
                broken.PreviousTargetGuid = pendingRef.refGuid;
                broken.Reason = "ExternalUnresolvedOnCreate";
                outReport->BrokenRefs.push_back(broken);
            }

            if (pendingRef.ptrToPtr == nullptr)
            {
                continue;
            }

            if (pendingRef.isRawPointer)
            {
                *static_cast<void**>(pendingRef.ptrToPtr) = nullptr;
            }
            else if (pendingRef.expectedClass != nullptr)
            {
                pendingRef.expectedClass->SetSharedPtr(std::shared_ptr<void>{}, pendingRef.ptrToPtr);
            }
        }

        unresolvedRefs.clear();
    }

    void PrefabUtilityDetail::FinalizeSceneObjects(Scene& scene)
    {
        scene.RebuildRuntimeGameObjectIndex();
        scene.ResolveGameObjectHierarchy();
        SceneManager::RebuildSceneComponentAttachHierarchy(&scene);
        if (SceneManager::HasInstance())
        {
            SceneManager::Get().ResolvePendingActivationsForScene(&scene);
        }
    }

    void PrefabUtilityDetail::RestorePreviousTemplates(
        Prefab& prefab,
        const std::vector<std::shared_ptr<GameObject>>& previousTemplates,
        const GUID& previousRootGuid)
    {
        prefab.ClearTemplateObjects();
        for (const std::shared_ptr<GameObject>& previous : previousTemplates)
        {
            if (!previous)
            {
                continue;
            }

            prefab.AddTemplateObject(previous);
            if (ObjectManager::HasInstance())
            {
                ObjectManager::Get().RegisterObject(previous);
                for (const std::shared_ptr<Component>& component : previous->GetAllComponents())
                {
                    if (component)
                    {
                        ObjectManager::Get().RegisterObject(component);
                    }
                }
            }
        }
        prefab.SetRootGuid(previousRootGuid);
    }

    void PrefabUtilityDetail::RefreshEditCloneMapAfterWrite(
        Scene& stageScene,
        const ObjectCloneContext& writeContext,
        ObjectCloneContext& inOutEditMap)
    {
        inOutEditMap.Clear();
        for (const auto& pair : writeContext.SourceToClonedGuid)
        {
            const GUID& stageGuid = pair.first;
            const GUID& templateGuid = pair.second;
            std::shared_ptr<MEObject> stageObject =
                PrefabObjectLookup::FindSharedInScene(stageScene, stageGuid);
            if (!stageObject && ObjectManager::HasInstance())
            {
                stageObject = ObjectManager::Get().FindObject(stageGuid);
            }
            if (stageObject)
            {
                inOutEditMap.RecordClone(templateGuid, stageObject, stageGuid);
            }
        }
    }
}
