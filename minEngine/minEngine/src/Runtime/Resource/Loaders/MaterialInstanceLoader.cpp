#include "Runtime/Resource/Loaders/MaterialInstanceLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Render/MaterialInstance.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Resource/AssetSerializeHelper.h"

namespace minEngine
{
    std::shared_ptr<MaterialInstance> MaterialInstanceLoader::Load(
        const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<MaterialInstance> instance =
            NewObject<MaterialInstance>(meta.AssetName, nullptr, meta.Guid);

        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        if (!AssetSerializeHelper::ReadObjectFromFile(absoluteAssetPath, *instance, outError))
        {
            return nullptr;
        }

        AssetManager::Get().ApplyMetaIdentity(*instance, meta);

        std::string finalizeError;
        if (!instance->FinalizeAfterLoad(&finalizeError))
        {
            if (outError)
            {
                *outError = finalizeError;
            }
            ME_LOG(LogAsset, Error,
                "MaterialInstanceLoader: finalize failed for '{}': {}",
                meta.AssetPath,
                finalizeError);
            return nullptr;
        }

        return instance;
    }

    bool MaterialInstanceLoader::Save(
        const AssetMeta& meta, const MaterialInstance& asset, std::string* outError)
    {
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();
        return AssetSerializeHelper::WriteObjectToFile(absoluteAssetPath, asset, outError);
    }

    bool MaterialInstanceLoader::SaveHandler(
        const AssetManager& /*manager*/,
        const AssetMeta& meta,
        const Asset& asset,
        std::string& outError)
    {
        const MaterialInstance* instance = dynamic_cast<const MaterialInstance*>(&asset);
        if (instance == nullptr)
        {
            outError = "SaveHandler: expected MaterialInstance";
            return false;
        }
        return Save(meta, *instance, &outError);
    }

    std::shared_ptr<Asset> MaterialInstanceLoader::CreateHandler(
        AssetManager& manager,
        const std::string& assetName,
        const std::string& directoryRel,
        std::string& outError)
    {
        std::shared_ptr<Material> parent;
        if (const AssetMeta* defaultMeta = manager.FindAssetMetaByPath("Materials/DefaultMaterial.memtl"))
        {
            parent = manager.LoadAsset<Material>(defaultMeta->AssetPath);
        }
        if (!parent)
        {
            const std::vector<const AssetMeta*> materials = manager.FindAssetMetasByType("Material");
            for (const AssetMeta* materialMeta : materials)
            {
                if (materialMeta == nullptr)
                {
                    continue;
                }
                parent = manager.LoadAsset<Material>(materialMeta->AssetPath);
                if (parent)
                {
                    break;
                }
            }
        }
        if (!parent)
        {
            ME_LOG(LogAsset, Error, "MaterialInstanceLoader::Create: no Material parent available.");
            return nullptr;
        }

        const std::string relativePath =
            manager.AllocateUniqueAssetRelativePath(directoryRel, assetName, ".memi");
        if (relativePath.empty())
        {
            outError = "failed to allocate unique path";
            ME_LOG(LogAsset, Error, "MaterialInstanceLoader::Create: failed to allocate unique path for '{}'.", assetName);
            return nullptr;
        }

        const std::filesystem::path absolutePath = manager.ResolveAssetAbsolutePath(relativePath);
        std::error_code createError;
        std::filesystem::create_directories(absolutePath.parent_path(), createError);
        if (createError)
        {
            ME_LOG(LogAsset, Error,
                "MaterialInstanceLoader::Create: failed to create directory '{}': {}",
                absolutePath.parent_path().string(),
                createError.message());
            return nullptr;
        }

        const std::string instanceName = absolutePath.stem().string();
        const GUID assetGuid = GenerateGUID();
        std::shared_ptr<MaterialInstance> instance = NewObject<MaterialInstance>(instanceName, nullptr, assetGuid);
        if (!instance->InitializeParent(parent))
        {
            return nullptr;
        }

        if (!manager.SaveRegisteredAsset(
                AssetManager::MakePendingSaveMeta(relativePath, instanceName, "MaterialInstance", assetGuid),
                *instance))
        {
            std::error_code removeError;
            std::filesystem::remove(absolutePath, removeError);
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(absolutePath);

        AssetMeta meta = manager.RegisterAsset(relativePath, "MaterialInstance", &assetGuid);
        if (meta.AssetPath.empty())
        {
            ME_LOG(LogAsset, Error, "MaterialInstanceLoader::Create: RegisterAsset failed for '{}'.", relativePath);
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(manager.BuildMetaAbsolutePath(meta.AssetPath));
        instance->SetMeta(const_cast<AssetMeta*>(manager.FindAssetMetaByGuid(assetGuid)));
        ME_LOG(LogAsset, Info, "MaterialInstanceLoader::Create: created '{}'.", meta.AssetPath);
        manager.CacheCreatedAsset(meta.AssetPath, instance);
        return std::static_pointer_cast<Asset>(instance);
    }

}
