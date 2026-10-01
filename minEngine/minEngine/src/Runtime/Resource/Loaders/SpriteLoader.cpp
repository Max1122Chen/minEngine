#include "Runtime/Resource/Loaders/SpriteLoader.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetSerializeHelper.h"

namespace minEngine
{
    std::shared_ptr<Sprite> SpriteLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<Sprite> sprite = NewObject<Sprite>(meta.AssetName, nullptr, meta.Guid);

        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        if (!AssetSerializeHelper::ReadObjectFromFile(absoluteAssetPath, *sprite, outError))
        {
            return nullptr;
        }

        AssetManager::Get().ApplyMetaIdentity(*sprite, meta);
        return sprite;
    }

    bool SpriteLoader::Save(const AssetMeta& meta, const Sprite& asset, std::string* outError)
    {
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        if (!AssetSerializeHelper::WriteObjectToFile(absoluteAssetPath, asset, outError))
        {
            return false;
        }

        return true;
    }

    bool SpriteLoader::SaveHandler(
        const AssetManager& /*manager*/,
        const AssetMeta& meta,
        const Asset& asset,
        std::string& outError)
    {
        const Sprite* sprite = dynamic_cast<const Sprite*>(&asset);
        if (sprite == nullptr)
        {
            outError = "SaveHandler: expected Sprite";
            return false;
        }
        return Save(meta, *sprite, &outError);
    }

    std::shared_ptr<SpriteAtlas> SpriteAtlasLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<SpriteAtlas> atlas = NewObject<SpriteAtlas>(meta.AssetName, nullptr, meta.Guid);

        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        if (!AssetSerializeHelper::ReadObjectFromFile(absoluteAssetPath, *atlas, outError))
        {
            return nullptr;
        }

        AssetManager::Get().ApplyMetaIdentity(*atlas, meta);
        atlas->RebindMemberOwnersAfterLoad();
        return atlas;
    }

    bool SpriteAtlasLoader::Save(const AssetMeta& meta, const SpriteAtlas& asset, std::string* outError)
    {
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        return AssetSerializeHelper::WriteObjectToFile(absoluteAssetPath, asset, outError);
    }

    bool SpriteAtlasLoader::SaveHandler(
        const AssetManager& /*manager*/,
        const AssetMeta& meta,
        const Asset& asset,
        std::string& outError)
    {
        const SpriteAtlas* atlas = dynamic_cast<const SpriteAtlas*>(&asset);
        if (atlas == nullptr)
        {
            outError = "SaveHandler: expected SpriteAtlas";
            return false;
        }
        return Save(meta, *atlas, &outError);
    }

    std::shared_ptr<Asset> SpriteLoader::CreateHandler(
        AssetManager& manager,
        const std::string& assetName,
        const std::string& directoryRel,
        std::string& outError)
    {
        const std::string relativePath =
            manager.AllocateUniqueAssetRelativePath(directoryRel, assetName, ".mesprite");
        if (relativePath.empty())
        {
            outError = "failed to allocate unique path";
            ME_LOG(LogAsset, Error, "SpriteLoader::Create: failed to allocate unique path for '{}'.", assetName);
            return nullptr;
        }

        const std::filesystem::path absolutePath = manager.ResolveAssetAbsolutePath(relativePath);
        std::error_code createError;
        std::filesystem::create_directories(absolutePath.parent_path(), createError);
        if (createError)
        {
            ME_LOG(LogAsset, Error,
                "SpriteLoader::Create: failed to create directory '{}': {}",
                absolutePath.parent_path().string(),
                createError.message());
            return nullptr;
        }

        const std::string spriteName = absolutePath.stem().string();
        const GUID assetGuid = GenerateGUID();
        std::shared_ptr<Sprite> sprite = NewObject<Sprite>(spriteName, nullptr, assetGuid);

        if (!manager.SaveRegisteredAsset(AssetManager::MakePendingSaveMeta(relativePath, spriteName, "Sprite", assetGuid), *sprite))
        {
            std::error_code removeError;
            std::filesystem::remove(absolutePath, removeError);
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(absolutePath);

        AssetMeta meta = manager.RegisterAsset(relativePath, "Sprite", &assetGuid);
        if (meta.AssetPath.empty())
        {
            ME_LOG(LogAsset, Error, "SpriteLoader::Create: RegisterAsset failed for '{}'.", relativePath);
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(manager.BuildMetaAbsolutePath(meta.AssetPath));
        sprite->SetMeta(const_cast<AssetMeta*>(manager.FindAssetMetaByGuid(assetGuid)));
        ME_LOG(LogAsset, Info, "SpriteLoader::Create: created '{}'.", meta.AssetPath);
        manager.CacheCreatedAsset(meta.AssetPath, sprite);
        return std::static_pointer_cast<Asset>(sprite);
    }


    std::shared_ptr<Asset> SpriteAtlasLoader::CreateHandler(
        AssetManager& manager,
        const std::string& assetName,
        const std::string& directoryRel,
        std::string& outError)
    {
        const std::string relativePath =
            manager.AllocateUniqueAssetRelativePath(directoryRel, assetName, ".mespriteatlas");
        if (relativePath.empty())
        {
            outError = "failed to allocate unique path";
            ME_LOG(LogAsset, Error, "SpriteAtlasLoader::Create: failed to allocate unique path for '{}'.", assetName);
            return nullptr;
        }

        const std::filesystem::path absolutePath = manager.ResolveAssetAbsolutePath(relativePath);
        std::error_code createError;
        std::filesystem::create_directories(absolutePath.parent_path(), createError);
        if (createError)
        {
            ME_LOG(LogAsset, Error,
                "SpriteAtlasLoader::Create: failed to create directory '{}': {}",
                absolutePath.parent_path().string(),
                createError.message());
            return nullptr;
        }

        const std::string atlasName = absolutePath.stem().string();
        const GUID assetGuid = GenerateGUID();
        std::shared_ptr<SpriteAtlas> atlas = NewObject<SpriteAtlas>(atlasName, nullptr, assetGuid);

        if (!manager.SaveRegisteredAsset(AssetManager::MakePendingSaveMeta(relativePath, atlasName, "SpriteAtlas", assetGuid), *atlas))
        {
            std::error_code removeError;
            std::filesystem::remove(absolutePath, removeError);
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(absolutePath);

        AssetMeta meta = manager.RegisterAsset(relativePath, "SpriteAtlas", &assetGuid);
        if (meta.AssetPath.empty())
        {
            ME_LOG(LogAsset, Error, "SpriteAtlasLoader::Create: RegisterAsset failed for '{}'.", relativePath);
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(manager.BuildMetaAbsolutePath(meta.AssetPath));
        atlas->SetMeta(const_cast<AssetMeta*>(manager.FindAssetMetaByGuid(assetGuid)));
        ME_LOG(LogAsset, Info, "SpriteAtlasLoader::Create: created '{}'.", meta.AssetPath);
        manager.CacheCreatedAsset(meta.AssetPath, atlas);
        return std::static_pointer_cast<Asset>(atlas);
    }

}
