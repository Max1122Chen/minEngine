#include "Runtime/Resource/Loaders/PrefabLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    std::shared_ptr<Prefab> PrefabLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<Prefab> prefab = NewObject<Prefab>(meta.AssetName, nullptr, meta.Guid);

        Serialization::JsonReaderArchive archive;
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        const Serialization::SerializeResult result = Serialization::Serializer::FromFile(
            absoluteAssetPath,
            prefab.get(),
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = false,
                .skipUnknownField = true,
            });

        if (!result.ok)
        {
            ObjectManager::Get().UnregisterObject(prefab.get());
            if (outError)
            {
                *outError = result.message;
            }
            ME_LOG(LogAsset, Error,
                "PrefabLoader: failed to deserialize '{}' — {} (field: {})",
                meta.AssetPath,
                result.message,
                result.fieldPath);
            return nullptr;
        }

        std::string validateError;
        if (!prefab->ValidateSingleRoot(&validateError))
        {
            ObjectManager::Get().UnregisterObject(prefab.get());
            if (outError)
            {
                *outError = validateError;
            }
            ME_LOG(LogAsset, Error, "PrefabLoader: invalid Prefab '{}': {}", meta.AssetPath, validateError);
            return nullptr;
        }

        return prefab;
    }

    template<>
    std::shared_ptr<Prefab> AssetManager::LoadAsset_Impl<Prefab>(const AssetMeta& meta)
    {
        return PrefabLoader::Load(meta);
    }
}
