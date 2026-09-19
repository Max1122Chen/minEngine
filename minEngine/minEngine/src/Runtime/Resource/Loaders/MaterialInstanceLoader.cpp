#include "Runtime/Resource/Loaders/MaterialInstanceLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Render/MaterialInstance.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    std::shared_ptr<MaterialInstance> MaterialInstanceLoader::Load(
        const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<MaterialInstance> instance =
            NewObject<MaterialInstance>(meta.AssetName, nullptr, meta.Guid);

        Serialization::JsonReaderArchive archive;
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        const Serialization::SerializeResult deserializeResult = Serialization::Serializer::FromFile(
            absoluteAssetPath,
            instance.get(),
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
                "MaterialInstanceLoader: deserialize failed for '{}' — {} (field: {})",
                meta.AssetPath,
                deserializeResult.message,
                deserializeResult.fieldPath);
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

    template<>
    std::shared_ptr<MaterialInstance> AssetManager::LoadAsset_Impl<MaterialInstance>(const AssetMeta& meta)
    {
        return MaterialInstanceLoader::Load(meta);
    }
}
