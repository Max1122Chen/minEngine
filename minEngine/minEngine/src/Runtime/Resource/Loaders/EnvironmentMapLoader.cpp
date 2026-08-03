#include "Runtime/Resource/Loaders/EnvironmentMapLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Render/Environment/EnvironmentMap.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RenderSystem.h"
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
            "minEngine::EnvironmentMap",
            environmentMap.get(),
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
                "EnvironmentMapLoader: deserialize failed for '{}' — {} (field: {})",
                meta.AssetPath,
                deserializeResult.message,
                deserializeResult.fieldPath);
            return nullptr;
        }

        RHI* rhi = RenderSystem::HasInstance() ? RenderSystem::Get().GetRHI() : nullptr;
        if (rhi == nullptr)
        {
            ME_CORE_WARN(
                "EnvironmentMapLoader: RHI unavailable while loading {}; GPU resources deferred.",
                meta.AssetPath);
            return environmentMap;
        }

        if (!environmentMap->EnsureGPUResources(*rhi))
        {
            if (outError)
            {
                *outError = "failed to create EnvironmentMap GPU resources";
            }
            ME_CORE_ERROR("EnvironmentMapLoader: EnsureGPUResources failed for {}.", meta.AssetPath);
            return nullptr;
        }

        return environmentMap;
    }

    template<>
    std::shared_ptr<EnvironmentMap> AssetManager::LoadAsset_Impl<EnvironmentMap>(const AssetMeta& meta)
    {
        return EnvironmentMapLoader::Load(meta);
    }
}
