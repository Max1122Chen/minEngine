#include "Runtime/Resource/Loaders/SceneLoader.h"

#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetSerializeHelper.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

namespace minEngine
{
    std::shared_ptr<Scene> SceneLoader::Load(const AssetMeta& meta)
    {
        std::shared_ptr<Scene> scene = NewObject<Scene>(meta.AssetName, nullptr, meta.Guid);
        scene->Reset();
        scene->SetSceneName(meta.AssetName);

        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        if (!AssetSerializeHelper::ReadObjectFromFile(absoluteAssetPath, *scene))
        {
            ObjectManager::Get().UnregisterObject(scene.get());
            return nullptr;
        }

        scene->SetSceneType(ESceneType::Editor);
        scene->SetTickPolicy(ESceneTickPolicy::Gameplay);
        scene->SetPIEInstanceId(-1);
        SceneManager::FinalizeLoadedScene(scene.get());
        return scene;
    }

    bool SceneLoader::Save(const AssetMeta& meta, const Scene& asset, std::string* outError)
    {
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();
        return AssetSerializeHelper::WriteObjectToFile(absoluteAssetPath, asset, outError);
    }

    bool SceneLoader::SaveHandler(
        const AssetManager& /*manager*/,
        const AssetMeta& meta,
        const Asset& asset,
        std::string& outError)
    {
        const Scene* scene = dynamic_cast<const Scene*>(&asset);
        if (scene == nullptr)
        {
            outError = "SaveHandler: expected Scene";
            return false;
        }
        return Save(meta, *scene, &outError);
    }

    std::shared_ptr<Asset> SceneLoader::CreateHandler(
        AssetManager& manager,
        const std::string& assetName,
        const std::string& directoryRel,
        std::string& outError)
    {
        const std::string relativePath =
            manager.AllocateUniqueAssetRelativePath(directoryRel, assetName, ".mescene");
        if (relativePath.empty())
        {
            outError = "failed to allocate unique path";
            ME_LOG(LogAsset, Error, "SceneLoader::Create: failed to allocate unique path for '{}'.", assetName);
            return nullptr;
        }

        const std::filesystem::path absolutePath = manager.ResolveAssetAbsolutePath(relativePath);
        std::error_code createError;
        std::filesystem::create_directories(absolutePath.parent_path(), createError);
        if (createError)
        {
            ME_LOG(LogAsset, Error, 
                "SceneLoader::Create: failed to create directory '{}': {}",
                absolutePath.parent_path().string(),
                createError.message());
            return nullptr;
        }

        const std::string sceneName = absolutePath.stem().string();
        const GUID assetGuid = GenerateGUID();
        std::shared_ptr<Scene> scene = NewObject<Scene>(sceneName, nullptr, assetGuid);
        scene->Reset();
        scene->SetSceneName(sceneName);
        scene->EnsureRenderScene();

        if (!manager.SaveRegisteredAsset(AssetManager::MakePendingSaveMeta(relativePath, sceneName, "Scene", assetGuid), *scene))
        {
            std::error_code removeError;
            std::filesystem::remove(absolutePath, removeError);
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(absolutePath);

        AssetMeta meta = manager.RegisterAsset(relativePath, "Scene", &assetGuid);
        if (meta.AssetPath.empty())
        {
            ME_LOG(LogAsset, Error, "SceneLoader::Create: RegisterAsset failed for '{}'.", relativePath);
            return nullptr;
        }

        if (SceneManager::HasInstance())
        {
            SceneManager::Get().RegisterScene(meta.AssetName, meta.AssetPath);
        }

        manager.NoteEditorFilesystemMutation(manager.BuildMetaAbsolutePath(meta.AssetPath));

        ME_LOG(LogAsset, Info, "SceneLoader::Create: created '{}'.", meta.AssetPath);
        manager.CacheCreatedAsset(meta.AssetPath, scene);
        return std::static_pointer_cast<Asset>(scene);
    }

}
