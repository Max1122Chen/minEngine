#include "Runtime/Resource/Loaders/SkeletonLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    std::shared_ptr<Skeleton> SkeletonLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<Skeleton> skeleton = NewObject<Skeleton>(meta.AssetName, nullptr, meta.Guid);

        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        Serialization::JsonReaderArchive archive;
        const Serialization::SerializeResult deserializeResult = Serialization::Serializer::FromFile(
            absoluteAssetPath,
            minEngine::Reflection::GetClassName<Skeleton>(),
            skeleton.get(),
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = true
                });

        if (!deserializeResult.ok)
        {
            if (outError != nullptr)
            {
                *outError = deserializeResult.message;
            }
            ME_LOG(LogAsset, Error, 
                "SkeletonLoader: deserialize failed for '{}' — {} (field: {})",
                meta.AssetPath,
                deserializeResult.message,
                deserializeResult.fieldPath);
            return nullptr;
        }

        // File may embed a stale m_Guid; registry identity comes from .meta.
        AssetManager::Get().ApplyMetaIdentity(*skeleton, meta);

        std::vector<SkeletonBone> loadedBones = skeleton->GetBones();
        std::string validateError;
        if (!skeleton->SetBones(std::move(loadedBones), &validateError))
        {
            if (outError != nullptr)
            {
                *outError = validateError;
            }
            ME_LOG(LogAsset, Error, 
                "SkeletonLoader: invalid bone data for '{}': {}",
                meta.AssetPath,
                validateError);
            return nullptr;
        }

        return skeleton;
    }

    bool SkeletonLoader::Save(const AssetMeta& meta, const Skeleton& skeleton, std::string* outError)
    {
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult serializeResult = Serialization::Serializer::ToFile(
            absoluteAssetPath,
            minEngine::Reflection::GetClassName<Skeleton>(),
            &skeleton,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = false
                });

        if (!serializeResult.ok)
        {
            if (outError != nullptr)
            {
                *outError = serializeResult.message;
            }
            ME_LOG(LogAsset, Error, 
                "SkeletonLoader: serialize failed for '{}' — {} (field: {})",
                meta.AssetPath,
                serializeResult.message,
                serializeResult.fieldPath);
            return false;
        }

        return true;
    }
}
