#include "Runtime/Resource/Loaders/SpriteLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    std::shared_ptr<Sprite> SpriteLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<Sprite> sprite = NewObject<Sprite>(meta.AssetName, nullptr, meta.Guid);

        Serialization::JsonReaderArchive archive;
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        const Serialization::SerializeResult deserializeResult = Serialization::Serializer::FromFile(
            absoluteAssetPath,
            sprite.get(),
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = false,
                .skipUnknownField = true,
            });
        if (!deserializeResult.ok)
        {
            if (outError)
            {
                *outError = deserializeResult.message;
            }
            ME_LOG(LogAsset, Error,
                "SpriteLoader: deserialize failed for '{}' — {} (field: {})",
                meta.AssetPath,
                deserializeResult.message,
                deserializeResult.fieldPath);
            return nullptr;
        }

        AssetManager::Get().ApplyMetaIdentity(*sprite, meta);
        return sprite;
    }

    bool SpriteLoader::Save(const AssetMeta& meta, const Sprite& asset, std::string* outError)
    {
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
            absoluteAssetPath,
            &asset,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = false,
                .skipUnknownField = false,
            });
        if (!result.ok)
        {
            if (outError)
            {
                *outError = result.message;
            }
            ME_LOG(LogAsset, Error,
                "SpriteLoader: serialize failed for '{}' — {} (field: {})",
                meta.AssetPath,
                result.message,
                result.fieldPath);
            return false;
        }

        return true;
    }

    std::shared_ptr<SpriteAtlas> SpriteAtlasLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<SpriteAtlas> atlas = NewObject<SpriteAtlas>(meta.AssetName, nullptr, meta.Guid);

        Serialization::JsonReaderArchive archive;
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        const Serialization::SerializeResult deserializeResult = Serialization::Serializer::FromFile(
            absoluteAssetPath,
            atlas.get(),
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = false,
                .skipUnknownField = true,
            });
        if (!deserializeResult.ok)
        {
            if (outError)
            {
                *outError = deserializeResult.message;
            }
            ME_LOG(LogAsset, Error,
                "SpriteAtlasLoader: deserialize failed for '{}' — {} (field: {})",
                meta.AssetPath,
                deserializeResult.message,
                deserializeResult.fieldPath);
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

        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
            absoluteAssetPath,
            &asset,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = false,
                .skipUnknownField = false,
            });
        if (!result.ok)
        {
            if (outError)
            {
                *outError = result.message;
            }
            ME_LOG(LogAsset, Error,
                "SpriteAtlasLoader: serialize failed for '{}' — {} (field: {})",
                meta.AssetPath,
                result.message,
                result.fieldPath);
            return false;
        }

        return true;
    }

    template<>
    std::shared_ptr<Sprite> AssetManager::LoadAsset_Impl<Sprite>(const AssetMeta& meta)
    {
        return SpriteLoader::Load(meta);
    }

    template<>
    std::shared_ptr<SpriteAtlas> AssetManager::LoadAsset_Impl<SpriteAtlas>(const AssetMeta& meta)
    {
        return SpriteAtlasLoader::Load(meta);
    }
}
