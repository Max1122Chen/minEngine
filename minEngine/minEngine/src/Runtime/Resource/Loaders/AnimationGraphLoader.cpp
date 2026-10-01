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
            ME_LOG(LogAsset, Error, 
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
            ME_LOG(LogAsset, Error, 
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
            ME_LOG(LogAsset, Warn, 
                "AnimationGraphLoader: '{}' loaded with validation warning: {}",
                meta.AssetPath,
                validateError);
        }

        return graph;
    }

    bool AnimationGraphLoader::SaveHandler(
        const AssetManager& /*manager*/,
        const AssetMeta& meta,
        const Asset& asset,
        std::string& outError)
    {
        const AnimationGraph* graph = dynamic_cast<const AnimationGraph*>(&asset);
        if (graph == nullptr)
        {
            outError = "SaveHandler: expected AnimationGraph";
            return false;
        }
        return Save(meta, *graph, &outError);
    }

    std::shared_ptr<Asset> AnimationGraphLoader::CreateHandler(
        AssetManager& manager,
        const std::string& assetName,
        const std::string& directoryRel,
        std::string& outError)
    {
        const std::string relativePath =
            manager.AllocateUniqueAssetRelativePath(directoryRel, assetName, ".meagraph");
        if (relativePath.empty())
        {
            outError = "failed to allocate unique path";
            ME_LOG(LogAsset, Error, "AnimationGraphLoader::Create: failed to allocate unique path for '{}'.", assetName);
            return nullptr;
        }

        const std::filesystem::path absolutePath = manager.ResolveAssetAbsolutePath(relativePath);
        std::error_code createError;
        std::filesystem::create_directories(absolutePath.parent_path(), createError);
        if (createError)
        {
            ME_LOG(LogAsset, Error, 
                "AnimationGraphLoader::Create: failed to create directory '{}': {}",
                absolutePath.parent_path().string(),
                createError.message());
            return nullptr;
        }

        const std::string graphName = absolutePath.stem().string();
        const GUID assetGuid = GenerateGUID();
        std::shared_ptr<AnimationGraph> graph = NewObject<AnimationGraph>(graphName, nullptr, assetGuid);

        if (!manager.SaveRegisteredAsset(
                AssetManager::MakePendingSaveMeta(relativePath, graphName, "AnimationGraph", assetGuid),
                *graph))
        {
            std::error_code removeError;
            std::filesystem::remove(absolutePath, removeError);
            ME_LOG(LogAsset, Error, "AnimationGraphLoader::Create: save failed.");
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(absolutePath);

        AssetMeta meta = manager.RegisterAsset(relativePath, "AnimationGraph", &assetGuid);
        if (meta.AssetPath.empty())
        {
            ME_LOG(LogAsset, Error, "AnimationGraphLoader::Create: RegisterAsset failed for '{}'.", relativePath);
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(manager.BuildMetaAbsolutePath(meta.AssetPath));

        ME_LOG(LogAsset, Info, "AnimationGraphLoader::Create: created '{}'.", meta.AssetPath);
        manager.CacheCreatedAsset(meta.AssetPath, graph);
        return std::static_pointer_cast<Asset>(graph);
    }

}
