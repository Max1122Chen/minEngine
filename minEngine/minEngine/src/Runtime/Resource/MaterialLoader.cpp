#include "MaterialLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    std::shared_ptr<Material> MaterialLoader::LoadDeserialized(const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<Material> material = NewObject<Material>(meta.AssetName, nullptr, meta.Guid);

        Serialization::JsonReaderArchive archive;
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        const Serialization::SerializeResult deserializeResult = Serialization::Serializer::FromFile(
            absoluteAssetPath,
            "minEngine::Material",
            material.get(),
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = false,
                .allowObjectPtrSerialization = true,
            });
        if (!deserializeResult.ok)
        {
            if (outError)
            {
                *outError = deserializeResult.message;
            }
            ME_CORE_ERROR(
                "MaterialLoader: deserialize failed for '{}' — {} (field: {})",
                meta.AssetPath,
                deserializeResult.message,
                deserializeResult.fieldPath);
            return nullptr;
        }

        std::string graphError;
        if (!material->FinalizeGraphAfterLoad(&graphError))
        {
            if (outError)
            {
                *outError = graphError;
            }
            ME_CORE_ERROR("MaterialLoader: finalize graph failed for '{}': {}", meta.AssetPath, graphError);
            return nullptr;
        }

        return material;
    }
}
