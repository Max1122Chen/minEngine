#include "Runtime/Resource/Loaders/EnvironmentMapLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Render/Environment/EnvironmentMap.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    std::shared_ptr<EnvironmentMap> EnvironmentMapLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<EnvironmentMap> environmentMap =
            NewObject<EnvironmentMap>(meta.AssetName, nullptr, meta.Guid);

        Serialization::JsonReaderArchive archive;
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        const Serialization::SerializeResult deserializeResult = Serialization::Serializer::FromFile(
            absoluteAssetPath,
            environmentMap.get(),
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
                "EnvironmentMapLoader: deserialize failed for '{}' — {} (field: {})",
                meta.AssetPath,
                deserializeResult.message,
                deserializeResult.fieldPath);
            return nullptr;
        }

        // Nothing GPU-side happens here. Loading runs on whichever thread asks for the asset, while
        // the bake creates textures and submits command buffers, which only the graphics role may
        // do: creating them here would leave every handle without a native half on any other
        // thread. The renderer asks for them at a frame start instead, and a frame that arrives
        // before that draws without a sky and says so.
        ME_LOG(LogAsset, Info, 
            "EnvironmentMapLoader: '{}' loaded; its GPU resources are created by the graphics role at "
            "a frame start.",
            meta.AssetPath);
        return environmentMap;
    }
}
