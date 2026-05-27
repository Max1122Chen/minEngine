#include "Runtime/Resource/Loaders/SceneLoader.h"

#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

namespace minEngine
{
    std::shared_ptr<Scene> SceneLoader::Load(const AssetMeta& meta)
    {
        std::shared_ptr<Scene> scene = NewObject<Scene>(meta.AssetName, nullptr, meta.Guid);
        scene->Reset();
        scene->m_SceneName = meta.AssetName;

        Serialization::JsonReaderArchive archive;
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        const Serialization::SerializeResult result = Serialization::Serializer::FromFile(
            absoluteAssetPath,
            minEngine::Reflection::GetClassName<Scene>(),
            scene.get(),
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = false,
                .allowObjectPtrSerialization = true,
            });

        if (!result.ok)
        {
            ObjectManager::Get().UnregisterObject(scene.get());
            ME_CORE_ERROR(
                "SceneLoader: failed to deserialize '{}' — {} (field: {})",
                meta.AssetPath,
                result.message,
                result.fieldPath);
            return nullptr;
        }

        scene->RebuildRuntimeGameObjectIndex();
        return scene;
    }

    template<>
    std::shared_ptr<Scene> AssetManager::LoadAsset_Impl<Scene>(const AssetMeta& meta)
    {
        return SceneLoader::Load(meta);
    }
}
