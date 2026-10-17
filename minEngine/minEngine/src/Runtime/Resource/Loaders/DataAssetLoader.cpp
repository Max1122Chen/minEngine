#include "Runtime/Resource/Loaders/DataAssetLoader.h"

#include "Runtime/Core/Object/ObjectCast.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetSerializeHelper.h"
#include "Runtime/Resource/DataAsset.h"

#include <filesystem>

namespace minEngine
{
    std::shared_ptr<DataAsset> DataAssetLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        if (meta.RuntimeClassName.empty())
        {
            if (outError != nullptr)
            {
                *outError = "DataAsset load requires RuntimeClassName in meta";
            }
            ME_LOG(
                LogAsset,
                Error,
                "DataAssetLoader::Load: missing RuntimeClassName for '{}'.",
                meta.AssetPath);
            return nullptr;
        }

        const Reflection::MEClass* classInfo =
            Reflection::ReflectionSystem::Get().FindClass(meta.RuntimeClassName);
        if (classInfo == nullptr)
        {
            if (outError != nullptr)
            {
                *outError = "DataAsset RuntimeClassName not found: " + meta.RuntimeClassName;
            }
            ME_LOG(
                LogAsset,
                Error,
                "DataAssetLoader::Load: class '{}' not found for '{}'.",
                meta.RuntimeClassName,
                meta.AssetPath);
            return nullptr;
        }

        if (!classInfo->IsA(DataAsset::StaticClass()))
        {
            if (outError != nullptr)
            {
                *outError = "RuntimeClassName is not a DataAsset: " + meta.RuntimeClassName;
            }
            ME_LOG(
                LogAsset,
                Error,
                "DataAssetLoader::Load: class '{}' is not a DataAsset for '{}'.",
                meta.RuntimeClassName,
                meta.AssetPath);
            return nullptr;
        }

        std::shared_ptr<MEObject> created =
            NewObject(classInfo, meta.AssetName, nullptr, meta.Guid);
        std::shared_ptr<DataAsset> asset = Cast<DataAsset>(created);
        if (!asset)
        {
            if (outError != nullptr)
            {
                *outError = "failed to construct DataAsset instance";
            }
            return nullptr;
        }

        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        if (!AssetSerializeHelper::ReadObjectFromFile(absoluteAssetPath, *asset, outError))
        {
            return nullptr;
        }

        AssetManager::Get().ApplyMetaIdentity(*asset, meta);
        return asset;
    }

    bool DataAssetLoader::Save(const AssetMeta& meta, const DataAsset& asset, std::string* outError)
    {
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        return AssetSerializeHelper::WriteObjectToFile(absoluteAssetPath, asset, outError);
    }

    bool DataAssetLoader::SaveHandler(
        const AssetManager& /*manager*/,
        const AssetMeta& meta,
        const Asset& asset,
        std::string& outError)
    {
        const DataAsset* dataAsset = Cast<DataAsset>(&asset);
        if (dataAsset == nullptr)
        {
            outError = "SaveHandler: expected DataAsset";
            return false;
        }
        return Save(meta, *dataAsset, &outError);
    }

    std::shared_ptr<Asset> DataAssetLoader::Create(
        AssetManager& manager,
        const Reflection::MEClass* classInfo,
        const std::string& assetName,
        const std::string& directoryRel,
        std::string& outError)
    {
        outError.clear();
        if (classInfo == nullptr)
        {
            outError = "CreateDataAsset: classInfo is null";
            return nullptr;
        }

        if (!classInfo->IsA(DataAsset::StaticClass()))
        {
            outError = "CreateDataAsset: class is not a DataAsset: " + classInfo->GetName();
            ME_LOG(LogAsset, Error, "DataAssetLoader::Create: {}", outError);
            return nullptr;
        }

        const std::string relativePath =
            manager.AllocateUniqueAssetRelativePath(directoryRel, assetName, ".medata");
        if (relativePath.empty())
        {
            outError = "failed to allocate unique path";
            ME_LOG(LogAsset, Error, "DataAssetLoader::Create: failed to allocate unique path for '{}'.", assetName);
            return nullptr;
        }

        const std::filesystem::path absolutePath = manager.ResolveAssetAbsolutePath(relativePath);
        std::error_code createError;
        std::filesystem::create_directories(absolutePath.parent_path(), createError);
        if (createError)
        {
            outError = createError.message();
            ME_LOG(
                LogAsset,
                Error,
                "DataAssetLoader::Create: failed to create directory '{}': {}",
                absolutePath.parent_path().string(),
                createError.message());
            return nullptr;
        }

        const std::string dataAssetName = absolutePath.stem().string();
        const GUID assetGuid = GenerateGUID();
        std::shared_ptr<MEObject> created = NewObject(classInfo, dataAssetName, nullptr, assetGuid);
        std::shared_ptr<DataAsset> dataAsset = Cast<DataAsset>(created);
        if (!dataAsset)
        {
            outError = "failed to construct DataAsset instance";
            return nullptr;
        }

        AssetMeta pendingMeta = AssetManager::MakePendingSaveMeta(
            relativePath, dataAssetName, "DataAsset", assetGuid);
        pendingMeta.RuntimeClassName = classInfo->GetName();

        if (!manager.SaveRegisteredAsset(pendingMeta, *dataAsset))
        {
            std::error_code removeError;
            std::filesystem::remove(absolutePath, removeError);
            outError = "failed to write DataAsset";
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(absolutePath);

        AssetMeta meta = manager.RegisterAsset(relativePath, "DataAsset", &assetGuid);
        if (meta.AssetPath.empty())
        {
            outError = "RegisterAsset failed";
            ME_LOG(LogAsset, Error, "DataAssetLoader::Create: RegisterAsset failed for '{}'.", relativePath);
            return nullptr;
        }

        if (meta.RuntimeClassName != classInfo->GetName())
        {
            AssetMeta* liveMeta = const_cast<AssetMeta*>(manager.FindAssetMetaByPath(meta.AssetPath));
            if (liveMeta != nullptr)
            {
                liveMeta->RuntimeClassName = classInfo->GetName();
                if (!manager.WriteOrUpdateMetaFile(*liveMeta))
                {
                    ME_LOG(
                        LogAsset,
                        Warn,
                        "DataAssetLoader::Create: failed to persist RuntimeClassName for '{}'.",
                        meta.AssetPath);
                }
                meta = *liveMeta;
            }
        }

        manager.NoteEditorFilesystemMutation(manager.BuildMetaAbsolutePath(meta.AssetPath));
        dataAsset->SetMeta(const_cast<AssetMeta*>(manager.FindAssetMetaByGuid(assetGuid)));
        ME_LOG(LogAsset, Info, "DataAssetLoader::Create: created '{}' ({}).", meta.AssetPath, classInfo->GetName());
        manager.CacheCreatedAsset(meta.AssetPath, dataAsset);
        return std::static_pointer_cast<Asset>(dataAsset);
    }

    std::shared_ptr<Asset> DataAssetLoader::CreateHandler(
        AssetManager& manager,
        const std::string& assetName,
        const std::string& directoryRel,
        std::string& outError)
    {
        return Create(manager, DataAsset::StaticClass(), assetName, directoryRel, outError);
    }
}
