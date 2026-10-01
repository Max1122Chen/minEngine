#include "Runtime/Resource/Loaders/PrefabLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Resource/AssetSerializeHelper.h"

namespace minEngine
{
    std::shared_ptr<Prefab> PrefabLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<Prefab> prefab = NewObject<Prefab>(meta.AssetName, nullptr, meta.Guid);

        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        if (!AssetSerializeHelper::ReadObjectFromFile(absoluteAssetPath, *prefab, outError))
        {
            ObjectManager::Get().UnregisterObject(prefab.get());
            return nullptr;
        }

        std::string validateError;
        if (!prefab->ValidateSingleRoot(&validateError))
        {
            ObjectManager::Get().UnregisterObject(prefab.get());
            if (outError)
            {
                *outError = validateError;
            }
            ME_LOG(LogAsset, Error, "PrefabLoader: invalid Prefab '{}': {}", meta.AssetPath, validateError);
            return nullptr;
        }

        return prefab;
    }

    bool PrefabLoader::Save(const AssetMeta& meta, const Prefab& asset, std::string* outError)
    {
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();
        return AssetSerializeHelper::WriteObjectToFile(absoluteAssetPath, asset, outError);
    }

    bool PrefabLoader::SaveHandler(
        const AssetManager& /*manager*/,
        const AssetMeta& meta,
        const Asset& asset,
        std::string& outError)
    {
        const Prefab* prefab = dynamic_cast<const Prefab*>(&asset);
        if (prefab == nullptr)
        {
            outError = "SaveHandler: expected Prefab";
            return false;
        }
        return Save(meta, *prefab, &outError);
    }

    std::shared_ptr<Asset> PrefabLoader::CreateHandler(
        AssetManager& manager,
        const std::string& assetName,
        const std::string& directoryRel,
        std::string& outError)
    {
        const std::string relativePath =
            manager.AllocateUniqueAssetRelativePath(directoryRel, assetName, ".meprefab");
        if (relativePath.empty())
        {
            outError = "failed to allocate unique path";
            ME_LOG(LogAsset, Error, "PrefabLoader::Create: failed to allocate unique path for '{}'.", assetName);
            return nullptr;
        }

        const std::filesystem::path absolutePath = manager.ResolveAssetAbsolutePath(relativePath);
        std::error_code createError;
        std::filesystem::create_directories(absolutePath.parent_path(), createError);
        if (createError)
        {
            ME_LOG(LogAsset, Error,
                "PrefabLoader::Create: failed to create directory '{}': {}",
                absolutePath.parent_path().string(),
                createError.message());
            return nullptr;
        }

        const std::string prefabName = absolutePath.stem().string();
        const GUID assetGuid = GenerateGUID();
        std::shared_ptr<Prefab> prefab = NewObject<Prefab>(prefabName, nullptr, assetGuid);
        std::shared_ptr<GameObject> rootObject = NewObject<GameObject>(prefabName, prefab.get());
        rootObject->AddComponent<SceneComponent>();
        prefab->AddTemplateObject(rootObject);
        prefab->SetRootGuid(rootObject->GetGuid());

        if (!manager.SaveRegisteredAsset(AssetManager::MakePendingSaveMeta(relativePath, prefabName, "Prefab", assetGuid), *prefab))
        {
            std::error_code removeError;
            std::filesystem::remove(absolutePath, removeError);
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(absolutePath);

        AssetMeta meta = manager.RegisterAsset(relativePath, "Prefab", &assetGuid);
        if (meta.AssetPath.empty())
        {
            ME_LOG(LogAsset, Error, "PrefabLoader::Create: RegisterAsset failed for '{}'.", relativePath);
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(manager.BuildMetaAbsolutePath(meta.AssetPath));

        ME_LOG(LogAsset, Info, "PrefabLoader::Create: created '{}'.", meta.AssetPath);
        manager.CacheCreatedAsset(meta.AssetPath, prefab);
        return std::static_pointer_cast<Asset>(prefab);
    }

}
