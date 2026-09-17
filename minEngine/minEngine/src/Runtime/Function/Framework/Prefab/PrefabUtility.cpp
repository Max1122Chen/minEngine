#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectCloneContext.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Function/Framework/Prefab/PrefabObjectLookup.h"
#include "Runtime/Function/Framework/Prefab/PrefabReferencePolicy.h"
#include "Runtime/Function/Framework/Prefab/PrefabUtilityDetail.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/EditorFilesystemMutationPass.h"

#include <filesystem>
#include <unordered_set>

namespace minEngine
{
    std::shared_ptr<Prefab> PrefabUtility::CreatePrefabFromGameObject(
        GameObject& root,
        PrefabCreateReport* outReport)
    {
        std::vector<GameObject*> subtree;
        PrefabUtilityDetail::CollectSubtree(root, subtree);
        if (subtree.empty())
        {
            ME_LOG(LogCore, Error, "PrefabUtility::CreatePrefabFromGameObject: empty subtree.");
            return nullptr;
        }

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

            std::shared_ptr<GameObject> cloned = PrefabUtilityDetail::CloneGameObjectAsTemplate(
                *sourceObject, cloneContext, unresolvedRefs, &error);
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
        if (!resolveResult.ok || !unresolvedRefs.empty())
        {
            PrefabUtilityDetail::ClearUnresolvedPendingRefs(unresolvedRefs, outReport);
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

        if (Scene* ownerScene = dynamic_cast<Scene*>(const_cast<MEObject*>(root.GetOuter())))
        {
            PrefabInstanceRecord record;
            PrefabUtilityDetail::BuildInstanceRecordFromCloneContext(
                record,
                prefab->GetGuid(),
                root.GetGuid(),
                cloneContext,
                PrefabUtilityDetail::EMappingDirection::SceneToTemplateInverted);
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
        const std::filesystem::path metaAbsolutePath =
            std::filesystem::path(absolutePath.string() + ".meta");

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

        // Swallow self-triggered watcher events (BUG-ASSET-001).
        EditorFilesystemMutationPass::NoteMutatedAbsolutePath(absolutePath.parent_path());
        EditorFilesystemMutationPass::NoteMutatedAbsolutePath(absolutePath);
        EditorFilesystemMutationPass::NoteMutatedAbsolutePath(metaAbsolutePath);

        // Existing registry entry: single write via SaveAsset (no duplicate ToFile).
        if (assetManager.FindAssetMetaByPath(projectRelativePath) != nullptr)
        {
            if (!assetManager.SaveAsset<Prefab>(projectRelativePath, prefab))
            {
                if (outError)
                {
                    *outError = "SaveAsset<Prefab> failed.";
                }
                return false;
            }
            return true;
        }

        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
            absolutePath.string(),
            &prefab,
            archive,
            PrefabUtilityDetail::kPrefabDiskOptions);
        if (!result.ok)
        {
            if (outError)
            {
                *outError = result.message;
            }
            return false;
        }

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
            std::shared_ptr<GameObject> cloned = PrefabUtilityDetail::CloneGameObjectAsTemplate(
                *templateObject, cloneContext, unresolvedRefs, &cloneError);
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

        PrefabUtilityDetail::FinalizeSceneObjects(targetScene);

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
            PrefabUtilityDetail::BuildInstanceRecordFromCloneContext(
                record,
                prefab.GetGuid(),
                instanceRoot->GetGuid(),
                cloneContext,
                PrefabUtilityDetail::EMappingDirection::TemplateToInstance);
            targetScene.GetPrefabInstancesMutable().push_back(std::move(record));
        }

        if (outCloneContext != nullptr)
        {
            *outCloneContext = cloneContext;
        }

        return instanceRoot;
    }

    bool PrefabUtility::IsTemplateObject(const Prefab& prefab, const GUID& objectGuid)
    {
        return PrefabObjectLookup::PrefabContains(prefab, objectGuid);
    }

    void PrefabUtility::RestoreTemplateObjects(
        Prefab& prefab,
        const std::vector<std::shared_ptr<GameObject>>& previousTemplates,
        const GUID& previousRootGuid)
    {
        PrefabUtilityDetail::RestorePreviousTemplates(prefab, previousTemplates, previousRootGuid);
    }

    bool PrefabUtility::IsEditorTempStageObject(const GameObject& gameObject)
    {
        const std::string& name = gameObject.GetName();
        return name.rfind(kEditorTempStageObjectNamePrefix, 0) == 0;
    }
}
