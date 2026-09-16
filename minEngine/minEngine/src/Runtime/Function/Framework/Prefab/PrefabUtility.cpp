#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectCloneContext.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Function/Framework/Prefab/PrefabReferencePolicy.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Resource/AssetManager.h"

#include <filesystem>
#include <unordered_set>

namespace minEngine
{
    namespace
    {
        constexpr Serialization::SerializerOptions kPrefabCloneOptions{
            .enumAsString = true,
            .strictTypeCheck = true,
            .skipUnknownField = false,
        };

        constexpr Serialization::SerializerOptions kPrefabDiskOptions{
            .enumAsString = true,
            .strictTypeCheck = false,
            .skipUnknownField = true,
        };

        void CollectSubtree(GameObject& root, std::vector<GameObject*>& outOrdered)
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

        std::shared_ptr<GameObject> CloneGameObjectAsTemplate(
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

            // Root object is filled in-place (not via Instanced ObjectPtr). Deserialize may copy the
            // source Guid onto the clone. RemapObjectGuid only Unregisters when the Guid slot still
            // points at *this* object, so the source GameObject stays registered.
            const GUID clonedGuid = GenerateGUID();
            cloneContext.RecordClone(sourceGuid, cloned, clonedGuid);
            ObjectManager::Get().RemapObjectGuid(cloned, clonedGuid);

            return cloned;
        }

        void BuildInstanceRecordFromCloneContext(
            PrefabInstanceRecord& record,
            const GUID& prefabAssetGuid,
            const GUID& rootInstanceGuid,
            const ObjectCloneContext& cloneContext)
        {
            record.PrefabAssetGuid = prefabAssetGuid;
            record.RootInstanceGuid = rootInstanceGuid;
            record.ObjectMappings.clear();
            record.Overrides.clear();

            for (const auto& pair : cloneContext.SourceToClonedGuid)
            {
                PrefabObjectMapping mapping;
                mapping.TemplateGuid = pair.first;
                mapping.InstanceGuid = pair.second;
                record.ObjectMappings.push_back(mapping);
            }
        }

        /** Create maps source(scene)→template; Invert for Template→Instance keep scene guids. */
        void BuildInstanceRecordAfterCreate(
            PrefabInstanceRecord& record,
            const GUID& prefabAssetGuid,
            const GUID& rootInstanceGuid,
            const ObjectCloneContext& createCloneContext)
        {
            record.PrefabAssetGuid = prefabAssetGuid;
            record.RootInstanceGuid = rootInstanceGuid;
            record.ObjectMappings.clear();
            record.Overrides.clear();

            for (const auto& pair : createCloneContext.SourceToClonedGuid)
            {
                PrefabObjectMapping mapping;
                mapping.TemplateGuid = pair.second;
                mapping.InstanceGuid = pair.first;
                record.ObjectMappings.push_back(mapping);
            }
        }

        void ClearUnresolvedPendingRefs(
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

        void FinalizeSceneObjects(Scene& scene)
        {
            scene.RebuildRuntimeGameObjectIndex();
            scene.ResolveGameObjectHierarchy();
            SceneManager::RebuildSceneComponentAttachHierarchy(&scene);
            if (SceneManager::HasInstance())
            {
                SceneManager::Get().ResolvePendingActivationsForScene(&scene);
            }
        }

        std::shared_ptr<MEObject> FindSharedObjectInScene(Scene& scene, const GUID& guid)
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

        bool TryRebuildEditCloneMapFromParallelTrees(
            Prefab& prefab,
            Scene& stageScene,
            ObjectCloneContext& outMap,
            std::string* outError)
        {
            outMap.Clear();

            GameObject* templateRoot = prefab.GetRootGameObject();
            if (templateRoot == nullptr)
            {
                if (outError)
                {
                    *outError = "Cannot rebuild EditCloneMap: Prefab has no root.";
                }
                return false;
            }

            std::shared_ptr<GameObject> stageRoot;
            size_t topLevelCount = 0;
            for (const std::shared_ptr<GameObject>& gameObject : stageScene.GetAllGameObjects())
            {
                if (gameObject && gameObject->GetParent() == nullptr)
                {
                    ++topLevelCount;
                    stageRoot = gameObject;
                }
            }

            if (topLevelCount != 1 || !stageRoot)
            {
                if (outError)
                {
                    *outError = "Cannot rebuild EditCloneMap: Stage must have exactly one top-level GameObject.";
                }
                return false;
            }

            std::vector<GameObject*> templateSubtree;
            std::vector<GameObject*> stageSubtree;
            CollectSubtree(*templateRoot, templateSubtree);
            CollectSubtree(*stageRoot, stageSubtree);
            if (templateSubtree.size() != stageSubtree.size())
            {
                if (outError)
                {
                    *outError = "Cannot rebuild EditCloneMap: Stage/template subtree size mismatch.";
                }
                return false;
            }

            for (size_t index = 0; index < templateSubtree.size(); ++index)
            {
                GameObject* templateObject = templateSubtree[index];
                GameObject* stageObject = stageSubtree[index];
                if (templateObject == nullptr || stageObject == nullptr)
                {
                    if (outError)
                    {
                        *outError = "Cannot rebuild EditCloneMap: null GameObject in subtree.";
                    }
                    return false;
                }

                std::shared_ptr<GameObject> stageShared =
                    std::dynamic_pointer_cast<GameObject>(FindSharedObjectInScene(stageScene, stageObject->GetGuid()));
                if (!stageShared)
                {
                    if (outError)
                    {
                        *outError = "Cannot rebuild EditCloneMap: Stage GameObject not held by Scene.";
                    }
                    return false;
                }

                outMap.RecordClone(templateObject->GetGuid(), stageShared, stageObject->GetGuid());

                const auto& templateComponents = templateObject->GetAllComponents();
                const auto& stageComponents = stageObject->GetAllComponents();
                if (templateComponents.size() != stageComponents.size())
                {
                    if (outError)
                    {
                        *outError = "Cannot rebuild EditCloneMap: component count mismatch.";
                    }
                    return false;
                }

                for (size_t componentIndex = 0; componentIndex < templateComponents.size(); ++componentIndex)
                {
                    const std::shared_ptr<Component>& templateComponent = templateComponents[componentIndex];
                    const std::shared_ptr<Component>& stageComponent = stageComponents[componentIndex];
                    if (!templateComponent || !stageComponent)
                    {
                        continue;
                    }

                    outMap.RecordClone(
                        templateComponent->GetGuid(),
                        stageComponent,
                        stageComponent->GetGuid());
                }
            }

            if (outMap.SourceToClonedGuid.find(prefab.GetRootGuid()) == outMap.SourceToClonedGuid.end())
            {
                if (outError)
                {
                    *outError = "Cannot rebuild EditCloneMap: rebuilt map still missing Prefab root.";
                }
                return false;
            }

            return true;
        }

        void RefreshEditCloneMapAfterWrite(
            Scene& stageScene,
            const ObjectCloneContext& writeContext,
            ObjectCloneContext& inOutEditMap)
        {
            inOutEditMap.Clear();
            for (const auto& pair : writeContext.SourceToClonedGuid)
            {
                const GUID& stageGuid = pair.first;
                const GUID& templateGuid = pair.second;
                std::shared_ptr<MEObject> stageObject = FindSharedObjectInScene(stageScene, stageGuid);
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

    std::shared_ptr<Prefab> PrefabUtility::CreatePrefabFromGameObject(
        GameObject& root,
        PrefabCreateReport* outReport)
    {
        std::vector<GameObject*> subtree;
        CollectSubtree(root, subtree);
        if (subtree.empty())
        {
            ME_LOG(LogCore, Error, "PrefabUtility::CreatePrefabFromGameObject: empty subtree.");
            return nullptr;
        }

        // Capture before clone/strip — external parent may leave local scale relative.
        const Transform sourceWorldTransform = root.GetWorldTransform();

        std::shared_ptr<Prefab> prefab = NewObject<Prefab>(root.GetName().empty() ? "Prefab" : root.GetName());
        prefab->ClearTemplateObjects();

        ObjectCloneContext cloneContext;
        std::vector<Serialization::PendingObjectRef> unresolvedRefs;
        std::string error;

        for (GameObject* sourceObject : subtree)
        {
            if (sourceObject == nullptr)
            {
                continue;
            }

            std::shared_ptr<GameObject> cloned =
                CloneGameObjectAsTemplate(*sourceObject, cloneContext, unresolvedRefs, &error);
            if (!cloned)
            {
                ME_LOG(LogCore, Error, "PrefabUtility::CreatePrefabFromGameObject: clone failed: {}", error);
                return nullptr;
            }

            cloned->SetOuter(prefab.get());
            prefab->AddTemplateObject(cloned);
        }

        Serialization::Serializer::SetActiveCloneContext(&cloneContext);
        const Serialization::SerializeResult resolveResult =
            Serialization::Serializer::ResolvePendingObjectRefs(unresolvedRefs);
        Serialization::Serializer::SetActiveCloneContext(nullptr);
        // Subtree Create may leave external parent/attach refs unresolved — clear and continue.
        if (!resolveResult.ok || !unresolvedRefs.empty())
        {
            ClearUnresolvedPendingRefs(unresolvedRefs, outReport);
        }

        const auto rootMapping = cloneContext.SourceToClonedGuid.find(root.GetGuid());
        if (rootMapping == cloneContext.SourceToClonedGuid.end())
        {
            ME_LOG(LogCore, Error, "PrefabUtility::CreatePrefabFromGameObject: missing root mapping.");
            return nullptr;
        }

        prefab->SetRootGuid(rootMapping->second);
        GameObject* templateRoot = prefab->GetRootGameObject();
        if (templateRoot != nullptr)
        {
            // Prefab roots are never parented outside the asset. Detach may be a no-op if the
            // external parent link was already cleared — always bake source world Transform.
            templateRoot->DetachFromParent(AttachmentTransformRules::KeepWorldTransform);
            templateRoot->SetWorldTransform(sourceWorldTransform);
        }

        std::unordered_set<GUID, GUID::Hash> templateGuidSet;
        for (const std::shared_ptr<GameObject>& templateObject : prefab->GetTemplateObjects())
        {
            if (!templateObject)
            {
                continue;
            }

            templateGuidSet.insert(templateObject->GetGuid());
            for (const std::shared_ptr<Component>& component : templateObject->GetAllComponents())
            {
                if (component)
                {
                    templateGuidSet.insert(component->GetGuid());
                }
            }
        }

        PrefabReferencePolicy::StripExternalNonAssetRefs(*prefab, templateGuidSet, outReport);

        std::string validateError;
        if (!prefab->ValidateSingleRoot(&validateError))
        {
            ME_LOG(LogCore, Error, "PrefabUtility::CreatePrefabFromGameObject: {}", validateError);
            return nullptr;
        }

        // Register empty-override PrefabInstance on the owning Scene (Create→instance link).
        if (Scene* ownerScene = dynamic_cast<Scene*>(const_cast<MEObject*>(root.GetOuter())))
        {
            PrefabInstanceRecord record;
            BuildInstanceRecordAfterCreate(record, prefab->GetGuid(), root.GetGuid(), cloneContext);
            ownerScene->GetPrefabInstancesMutable().push_back(std::move(record));
        }

        return prefab;
    }

    bool PrefabUtility::SavePrefabAsset(
        Prefab& prefab,
        const std::string& projectRelativePath,
        std::string* outError)
    {
        std::string validateError;
        if (!prefab.ValidateSingleRoot(&validateError))
        {
            if (outError)
            {
                *outError = validateError;
            }
            return false;
        }

        if (!AssetManager::HasInstance())
        {
            if (outError)
            {
                *outError = "AssetManager is not available.";
            }
            return false;
        }

        AssetManager& assetManager = AssetManager::Get();
        const std::filesystem::path absolutePath = assetManager.ResolveAssetAbsolutePath(projectRelativePath);
        std::error_code createError;
        std::filesystem::create_directories(absolutePath.parent_path(), createError);
        if (createError)
        {
            if (outError)
            {
                *outError = createError.message();
            }
            return false;
        }

        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
            absolutePath.string(),
            &prefab,
            archive,
            kPrefabDiskOptions);
        if (!result.ok)
        {
            if (outError)
            {
                *outError = result.message;
            }
            return false;
        }

        if (assetManager.FindAssetMetaByPath(projectRelativePath) == nullptr)
        {
            const GUID preferredGuid = prefab.GetGuid();

            AssetMeta pendingMeta;
            pendingMeta.AssetName = absolutePath.stem().string();
            pendingMeta.AssetPath = projectRelativePath;
            pendingMeta.AssetType = "Prefab";
            pendingMeta.Guid = preferredGuid;
            if (!assetManager.WriteOrUpdateMetaFile(pendingMeta))
            {
                if (outError)
                {
                    *outError = "Failed to write Prefab .meta before RegisterAsset.";
                }
                return false;
            }

            const AssetMeta meta = assetManager.RegisterAsset(projectRelativePath, "Prefab", &preferredGuid);
            if (meta.AssetPath.empty())
            {
                if (outError)
                {
                    *outError = "RegisterAsset failed for Prefab.";
                }
                return false;
            }

            if (meta.Guid != preferredGuid)
            {
                if (outError)
                {
                    *outError = "RegisterAsset returned a different Prefab Guid than the in-memory asset.";
                }
                return false;
            }
        }
        else if (!assetManager.SaveAsset<Prefab>(projectRelativePath, prefab))
        {
            if (outError)
            {
                *outError = "SaveAsset<Prefab> failed.";
            }
            return false;
        }

        return true;
    }

    std::shared_ptr<Prefab> PrefabUtility::LoadPrefabAsset(const GUID& assetGuid)
    {
        if (!AssetManager::HasInstance())
        {
            return nullptr;
        }

        std::string error;
        std::shared_ptr<Asset> asset = AssetManager::Get().LoadAssetByGUID(assetGuid, error);
        return std::dynamic_pointer_cast<Prefab>(asset);
    }

    std::shared_ptr<GameObject> PrefabUtility::Instantiate(
        const Prefab& prefab,
        Scene& targetScene,
        const PrefabInstantiateParams& params,
        std::string* outError)
    {
        return Instantiate(prefab, targetScene, params, nullptr, outError);
    }

    std::shared_ptr<GameObject> PrefabUtility::Instantiate(
        const Prefab& prefab,
        Scene& targetScene,
        const PrefabInstantiateParams& params,
        ObjectCloneContext* outCloneContext,
        std::string* outError)
    {
        std::string validateError;
        if (!prefab.ValidateSingleRoot(&validateError))
        {
            if (outError)
            {
                *outError = validateError;
            }
            return nullptr;
        }

        ObjectCloneContext cloneContext;
        std::vector<Serialization::PendingObjectRef> unresolvedRefs;
        std::vector<std::shared_ptr<GameObject>> clonedObjects;
        clonedObjects.reserve(prefab.GetTemplateObjects().size());

        for (const std::shared_ptr<GameObject>& templateObject : prefab.GetTemplateObjects())
        {
            if (!templateObject)
            {
                continue;
            }

            std::string cloneError;
            std::shared_ptr<GameObject> cloned =
                CloneGameObjectAsTemplate(*templateObject, cloneContext, unresolvedRefs, &cloneError);
            if (!cloned)
            {
                if (outError)
                {
                    *outError = cloneError;
                }
                return nullptr;
            }

            clonedObjects.push_back(cloned);
        }

        Serialization::Serializer::SetActiveCloneContext(&cloneContext);
        const Serialization::SerializeResult resolveResult =
            Serialization::Serializer::ResolvePendingObjectRefs(unresolvedRefs);
        Serialization::Serializer::SetActiveCloneContext(nullptr);
        if (!resolveResult.ok)
        {
            if (outError)
            {
                *outError = resolveResult.message;
            }
            return nullptr;
        }

        std::shared_ptr<GameObject> instanceRoot;
        const auto rootMapIter = cloneContext.SourceToClonedGuid.find(prefab.GetRootGuid());
        if (rootMapIter != cloneContext.SourceToClonedGuid.end())
        {
            const auto objectIter = cloneContext.ClonedBySourceGuid.find(prefab.GetRootGuid());
            if (objectIter != cloneContext.ClonedBySourceGuid.end())
            {
                instanceRoot = std::dynamic_pointer_cast<GameObject>(objectIter->second);
            }
        }

        for (const std::shared_ptr<GameObject>& cloned : clonedObjects)
        {
            targetScene.InsertRestoredGameObject(cloned);
        }

        if (!instanceRoot)
        {
            if (outError)
            {
                *outError = "Failed to locate instantiated Prefab root.";
            }
            return nullptr;
        }

        FinalizeSceneObjects(targetScene);

        // Stage / default: keep cloned template root pose. Level spawn: apply explicit placement.
        if (params.bApplyWorldTransform)
        {
            instanceRoot->SetWorldTransform(params.WorldTransform);
        }
        if (params.AttachParent != nullptr)
        {
            instanceRoot->AttachToParent(params.AttachParent, AttachmentTransformRules::KeepWorldTransform);
        }

        if (params.bRegisterPrefabInstance && targetScene.IsEditorScene())
        {
            PrefabInstanceRecord record;
            BuildInstanceRecordFromCloneContext(
                record,
                prefab.GetGuid(),
                instanceRoot->GetGuid(),
                cloneContext);
            targetScene.GetPrefabInstancesMutable().push_back(std::move(record));
        }

        if (outCloneContext != nullptr)
        {
            *outCloneContext = cloneContext;
        }

        return instanceRoot;
    }

    bool PrefabUtility::WriteStageTreeToPrefab(
        Scene& stageScene,
        Prefab& prefab,
        ObjectCloneContext& inOutEditMap,
        std::string* outError)
    {
        auto rootMapIter = inOutEditMap.SourceToClonedGuid.find(prefab.GetRootGuid());
        if (rootMapIter == inOutEditMap.SourceToClonedGuid.end())
        {
            ME_LOG(
                LogCore,
                Warn,
                "WriteStageTreeToPrefab: EditCloneMap missing root {} (map size={}); attempting rebuild.",
                prefab.GetRootGuid().ToString(),
                inOutEditMap.SourceToClonedGuid.size());

            std::string rebuildError;
            if (!TryRebuildEditCloneMapFromParallelTrees(prefab, stageScene, inOutEditMap, &rebuildError))
            {
                if (outError)
                {
                    *outError = rebuildError.empty()
                        ? "EditCloneMap missing Prefab root mapping; cannot Save without Guid table."
                        : rebuildError;
                }
                return false;
            }

            rootMapIter = inOutEditMap.SourceToClonedGuid.find(prefab.GetRootGuid());
            if (rootMapIter == inOutEditMap.SourceToClonedGuid.end())
            {
                if (outError)
                {
                    *outError = "EditCloneMap missing Prefab root mapping after rebuild.";
                }
                return false;
            }
        }

        const GUID stageRootGuid = rootMapIter->second;
        std::shared_ptr<GameObject> stageRoot =
            std::dynamic_pointer_cast<GameObject>(FindSharedObjectInScene(stageScene, stageRootGuid));
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

        size_t topLevelCount = 0;
        for (const std::shared_ptr<GameObject>& gameObject : stageScene.GetAllGameObjects())
        {
            if (gameObject && gameObject->GetParent() == nullptr)
            {
                ++topLevelCount;
            }
        }
        if (topLevelCount != 1)
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
        CollectSubtree(*stageRoot, subtree);

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
            std::shared_ptr<GameObject> cloned =
                CloneGameObjectAsTemplate(*stageObject, writeContext, unresolvedRefs, &cloneError);
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

        // Keep previous templates for rollback if Guid remap / resolve fails after Clear.
        std::vector<std::shared_ptr<GameObject>> previousTemplates = prefab.GetTemplateObjects();
        const GUID previousRootGuid = prefab.GetRootGuid();

        // Free previous template Guids before remapping new templates onto them.
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
            if (outError)
            {
                *outError = resolveResult.message;
            }
            return false;
        }

        const GUID newRootGuid = writeContext.SourceToClonedGuid[stageRootGuid];
        if (newRootGuid.IsZero())
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
            // Prefab roots stay unparented outside the asset.
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
            if (outError)
            {
                *outError = validateError;
            }
            return false;
        }

        // Refresh Template→Stage map from Scene-held objects (not ObjectManager alone).
        RefreshEditCloneMapAfterWrite(stageScene, writeContext, inOutEditMap);

        if (inOutEditMap.SourceToClonedGuid.find(prefab.GetRootGuid()) == inOutEditMap.SourceToClonedGuid.end())
        {
            ME_LOG(
                LogCore,
                Warn,
                "WriteStageTreeToPrefab: map refresh missed root {}; rebuilding from parallel trees.",
                prefab.GetRootGuid().ToString());

            std::string rebuildError;
            if (!TryRebuildEditCloneMapFromParallelTrees(prefab, stageScene, inOutEditMap, &rebuildError))
            {
                if (outError)
                {
                    *outError = rebuildError.empty()
                        ? "EditCloneMap lost Prefab root mapping after Save refresh."
                        : rebuildError;
                }
                return false;
            }
        }

        return true;
    }

    bool PrefabUtility::IsTemplateObject(const Prefab& prefab, const GUID& objectGuid)
    {
        if (objectGuid.IsZero())
        {
            return false;
        }

        for (const std::shared_ptr<GameObject>& templateObject : prefab.GetTemplateObjects())
        {
            if (!templateObject)
            {
                continue;
            }

            if (templateObject->GetGuid() == objectGuid)
            {
                return true;
            }

            for (const std::shared_ptr<Component>& component : templateObject->GetAllComponents())
            {
                if (component && component->GetGuid() == objectGuid)
                {
                    return true;
                }
            }
        }

        return false;
    }

    PrefabInstanceRecord* PrefabUtility::FindInstanceRecord(Scene& scene, const GUID& instanceObjectGuid)
    {
        for (PrefabInstanceRecord& record : scene.GetPrefabInstancesMutable())
        {
            if (record.RootInstanceGuid == instanceObjectGuid)
            {
                return &record;
            }

            for (const PrefabObjectMapping& mapping : record.ObjectMappings)
            {
                if (mapping.InstanceGuid == instanceObjectGuid)
                {
                    return &record;
                }
            }
        }

        return nullptr;
    }

    const PrefabInstanceRecord* PrefabUtility::FindInstanceRecord(
        const Scene& scene,
        const GUID& instanceObjectGuid)
    {
        return FindInstanceRecord(const_cast<Scene&>(scene), instanceObjectGuid);
    }
}
