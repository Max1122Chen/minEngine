#include "Runtime/Resource/Loaders/DataAssetLoader.h"

#include "Runtime/Core/Object/ObjectCast.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetSerializeHelper.h"
#include "Runtime/Resource/DataAsset.h"

#include <filesystem>

namespace minEngine
{
    std::shared_ptr<DataAsset> DataAssetLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<DataAsset> asset = NewObject<DataAsset>(meta.AssetName, nullptr, meta.Guid);

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

    std::shared_ptr<Asset> DataAssetLoader::CreateHandler(
        AssetManager& manager,
        const std::string& assetName,
        const std::string& directoryRel,
        std::string& outError)
    {
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
            ME_LOG(LogAsset, Error,
                "DataAssetLoader::Create: failed to create directory '{}': {}",
                absolutePath.parent_path().string(),
                createError.message());
            return nullptr;
        }

        const std::string dataAssetName = absolutePath.stem().string();
        const GUID assetGuid = GenerateGUID();
        std::shared_ptr<DataAsset> dataAsset = NewObject<DataAsset>(dataAssetName, nullptr, assetGuid);

        if (!manager.SaveRegisteredAsset(
                AssetManager::MakePendingSaveMeta(relativePath, dataAssetName, "DataAsset", assetGuid),
                *dataAsset))
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

        manager.NoteEditorFilesystemMutation(manager.BuildMetaAbsolutePath(meta.AssetPath));
        dataAsset->SetMeta(const_cast<AssetMeta*>(manager.FindAssetMetaByGuid(assetGuid)));
        ME_LOG(LogAsset, Info, "DataAssetLoader::Create: created '{}'.", meta.AssetPath);
        manager.CacheCreatedAsset(meta.AssetPath, dataAsset);
        return std::static_pointer_cast<Asset>(dataAsset);
    }
}
