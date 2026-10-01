#include "Runtime/Resource/Loaders/AnimationClipLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Animation/Skeleton.h"
#include "Runtime/Resource/AssetManager.h"


#include <algorithm>

namespace minEngine
{
    bool AnimationClipLoader::Save(const AssetMeta& meta, const AnimationClip& clip, std::string* outError)
    {
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult serializeResult = Serialization::Serializer::ToFile(
            absoluteAssetPath,
            Reflection::GetClassName<AnimationClip>(),
            &clip,
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
                "AnimationClipLoader: Save failed for '{}' — {} (field: {})",
                meta.AssetPath,
                serializeResult.message,
                serializeResult.fieldPath);
            return false;
        }
        return true;
    }

    std::shared_ptr<AnimationClip> AnimationClipLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<AnimationClip> clip = NewObject<AnimationClip>(meta.AssetName, nullptr, meta.Guid);

        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        Serialization::JsonReaderArchive archive;
        const Serialization::SerializeResult deserializeResult = Serialization::Serializer::FromFile(
            absoluteAssetPath,
            Reflection::GetClassName<AnimationClip>(),
            clip.get(),
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
                "AnimationClipLoader: Load failed for '{}' — {} (field: {})",
                meta.AssetPath,
                deserializeResult.message,
                deserializeResult.fieldPath);
            return nullptr;
        }

        AssetManager::Get().ApplyMetaIdentity(*clip, meta);

        if (clip->GetSkeleton() == nullptr)
        {
            ME_LOG(LogAsset, Warn, 
                "AnimationClipLoader: '{}' loaded without resolved Skeleton reference.",
                meta.AssetPath);
        }

        return clip;
    }
}
