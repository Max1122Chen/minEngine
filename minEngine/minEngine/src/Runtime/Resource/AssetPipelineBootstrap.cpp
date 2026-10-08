#include "AssetPipelineBootstrap.h"

#include "AssetManager.h"
#include "Runtime/Resource/Importers/AnimationClipImporter.h"
#include "Runtime/Resource/Importers/MeshImporter.h"
#include "Runtime/Resource/Importers/NativeCopyImporter.h"
#include "Runtime/Resource/Importers/Texture2DImporter.h"
#include "Runtime/Resource/Loaders/AnimationGraphLoader.h"
#include "Runtime/Resource/Loaders/MaterialInstanceLoader.h"
#include "Runtime/Resource/Loaders/MaterialLoader.h"
#include "Runtime/Resource/Loaders/PrefabLoader.h"
#include "Runtime/Resource/Loaders/SceneLoader.h"
#include "Runtime/Resource/Loaders/DataAssetLoader.h"
#include "Runtime/Resource/Loaders/SpriteLoader.h"
#include "Runtime/Resource/Loaders/Texture2DLoader.h"

namespace minEngine
{
    void RegisterAllAssetPipelines(AssetManager& assetManager)
    {
        assetManager.RegisterLoadHandler("StaticMesh", &AssetManager::LoadHandler_StaticMesh);
        assetManager.RegisterLoadHandler("SkeletalMesh", &AssetManager::LoadHandler_SkeletalMesh);
        assetManager.RegisterLoadHandler("Skeleton", &AssetManager::LoadHandler_Skeleton);
        assetManager.RegisterLoadHandler("AnimationClip", &AssetManager::LoadHandler_AnimationClip);
        assetManager.RegisterLoadHandler("AnimationGraph", &AssetManager::LoadHandler_AnimationGraph);
        assetManager.RegisterLoadHandler("Texture2D", &AssetManager::LoadHandler_Texture2D);
        assetManager.RegisterLoadHandler("Scene", &AssetManager::LoadHandler_Scene);
        assetManager.RegisterLoadHandler("Prefab", &AssetManager::LoadHandler_Prefab);
        assetManager.RegisterLoadHandler("Material", &AssetManager::LoadHandler_Material);
        assetManager.RegisterLoadHandler("MaterialInstance", &AssetManager::LoadHandler_MaterialInstance);
        assetManager.RegisterLoadHandler("Sprite", &AssetManager::LoadHandler_Sprite);
        assetManager.RegisterLoadHandler("SpriteAtlas", &AssetManager::LoadHandler_SpriteAtlas);
        assetManager.RegisterLoadHandler("DataAsset", &AssetManager::LoadHandler_DataAsset);
        assetManager.RegisterLoadHandler("Font", &AssetManager::LoadHandler_Font);
        assetManager.RegisterLoadHandler("LuaScript", &AssetManager::LoadHandler_LuaScript);
        assetManager.RegisterLoadHandler("EnvironmentMap", &AssetManager::LoadHandler_EnvironmentMap);
        assetManager.RegisterLoadHandler("AudioClip", &AssetManager::LoadHandler_AudioClip);
        assetManager.RegisterLoadHandler("Shader", &AssetManager::LoadHandler_ShaderRemoved);

        assetManager.RegisterSaveHandler("Scene", &SceneLoader::SaveHandler);
        assetManager.RegisterSaveHandler("Prefab", &PrefabLoader::SaveHandler);
        assetManager.RegisterSaveHandler("Material", &MaterialLoader::SaveHandler);
        assetManager.RegisterSaveHandler("MaterialInstance", &MaterialInstanceLoader::SaveHandler);
        assetManager.RegisterSaveHandler("Sprite", &SpriteLoader::SaveHandler);
        assetManager.RegisterSaveHandler("SpriteAtlas", &SpriteAtlasLoader::SaveHandler);
        assetManager.RegisterSaveHandler("DataAsset", &DataAssetLoader::SaveHandler);
        assetManager.RegisterSaveHandler("AnimationGraph", &AnimationGraphLoader::SaveHandler);
        assetManager.RegisterSaveHandler("Texture2D", &Texture2DLoader::SaveHandler);

        assetManager.RegisterCreateHandler("Scene", &SceneLoader::CreateHandler);
        assetManager.RegisterCreateHandler("Prefab", &PrefabLoader::CreateHandler);
        assetManager.RegisterCreateHandler("Material", &MaterialLoader::CreateHandler);
        assetManager.RegisterCreateHandler("MaterialInstance", &MaterialInstanceLoader::CreateHandler);
        assetManager.RegisterCreateHandler("Sprite", &SpriteLoader::CreateHandler);
        assetManager.RegisterCreateHandler("SpriteAtlas", &SpriteAtlasLoader::CreateHandler);
        assetManager.RegisterCreateHandler("DataAsset", &DataAssetLoader::CreateHandler);
        assetManager.RegisterCreateHandler("AnimationGraph", &AnimationGraphLoader::CreateHandler);
        assetManager.RegisterCreateHandler("Texture2D", &Texture2DLoader::CreateHandler);

        assetManager.RegisterImportProduct(ImportProductDescriptor{
            .ProductId = "Texture2D",
            .DisplayName = "Texture2D",
            .AcceptsSourceExtension = &Texture2DImporter::AcceptsSourceExtension,
            .bNeedsSkeletonPicker = false,
            .Import = &Texture2DImporter::Import});
        assetManager.RegisterImportProduct(ImportProductDescriptor{
            .ProductId = "NativeCopy",
            .DisplayName = "Native Asset (copy)",
            .AcceptsSourceExtension = &NativeCopyImporter::AcceptsSourceExtension,
            .bNeedsSkeletonPicker = false,
            .Import = &NativeCopyImporter::Import});
        assetManager.RegisterImportProduct(ImportProductDescriptor{
            .ProductId = "StaticMesh",
            .DisplayName = "Static Mesh",
            .AcceptsSourceExtension = &MeshImporter::AcceptsSourceExtension,
            .bNeedsSkeletonPicker = false,
            .Import = &MeshImporter::ImportStaticMesh});
        assetManager.RegisterImportProduct(ImportProductDescriptor{
            .ProductId = "SkeletalMesh",
            .DisplayName = "Skeletal Mesh",
            .AcceptsSourceExtension = &MeshImporter::AcceptsSourceExtension,
            .bNeedsSkeletonPicker = false,
            .Import = &MeshImporter::ImportSkeletalMesh});
        assetManager.RegisterImportProduct(ImportProductDescriptor{
            .ProductId = "AnimationClip",
            .DisplayName = "Animation Clip",
            .AcceptsSourceExtension = &AnimationClipImporter::AcceptsSourceExtension,
            .bNeedsSkeletonPicker = true,
            .Import = &AnimationClipImporter::Import});
    }
}
