#include "Runtime/Resource/Loaders/AnimationGraphLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    bool AnimationGraphLoader::Save(const AssetMeta& meta, const AnimationGraph& graph, std::string* outError)
    {
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult serializeResult = Serialization::Serializer::ToFile(
            absoluteAssetPath,
            Reflection::GetClassName<AnimationGraph>(),
            &graph,
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
            ME_CORE_ERROR(
                "AnimationGraphLoader: Save failed for '{}' - {} (field: {})",
                meta.AssetPath,
                serializeResult.message,
                serializeResult.fieldPath);
            return false;
        }
        return true;
    }

    std::shared_ptr<AnimationGraph> AnimationGraphLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<AnimationGraph> graph = NewObject<AnimationGraph>(meta.AssetName, nullptr, meta.Guid);

        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        Serialization::JsonReaderArchive archive;
        const Serialization::SerializeResult deserializeResult = Serialization::Serializer::FromFile(
            absoluteAssetPath,
            Reflection::GetClassName<AnimationGraph>(),
            graph.get(),
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
            ME_CORE_ERROR(
                "AnimationGraphLoader: Load failed for '{}' - {} (field: {})",
                meta.AssetPath,
                deserializeResult.message,
                deserializeResult.fieldPath);
            return nullptr;
        }

        AssetManager::Get().ApplyMetaIdentity(*graph, meta);

        std::string validateError;
        if (!graph->Validate(&validateError))
        {
            ME_CORE_WARN(
                "AnimationGraphLoader: '{}' loaded with validation warning: {}",
                meta.AssetPath,
                validateError);
        }

        return graph;
    }

    template <>
    std::shared_ptr<AnimationGraph> AssetManager::LoadAsset_Impl<AnimationGraph>(const AssetMeta& meta)
    {
        return AnimationGraphLoader::Load(meta);
    }
}

