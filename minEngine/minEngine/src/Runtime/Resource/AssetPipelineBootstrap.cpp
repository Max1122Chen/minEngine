#include "AssetPipelineBootstrap.h"

#include "AssetManager.h"

namespace minEngine
{
    void RegisterAllAssetPipelines(AssetManager& assetManager)
    {
        assetManager.RegisterLoadHandler("StaticMesh", &AssetManager::LoadHandler_StaticMesh);
        assetManager.RegisterLoadHandler("SkeletalMesh", &AssetManager::LoadHandler_SkeletalMesh);
        assetManager.RegisterLoadHandler("Skeleton", &AssetManager::LoadHandler_Skeleton);
        assetManager.RegisterLoadHandler("AnimationClip", &AssetManager::LoadHandler_AnimationClip);
        assetManager.RegisterLoadHandler("Texture2D", &AssetManager::LoadHandler_Texture2D);
        assetManager.RegisterLoadHandler("Scene", &AssetManager::LoadHandler_Scene);
        assetManager.RegisterLoadHandler("Material", &AssetManager::LoadHandler_Material);
        assetManager.RegisterLoadHandler("Font", &AssetManager::LoadHandler_Font);
        assetManager.RegisterLoadHandler("LuaScript", &AssetManager::LoadHandler_LuaScript);
        assetManager.RegisterLoadHandler("EnvironmentMap", &AssetManager::LoadHandler_EnvironmentMap);
        assetManager.RegisterLoadHandler("AudioClip", &AssetManager::LoadHandler_AudioClip);
        assetManager.RegisterLoadHandler("Shader", &AssetManager::LoadHandler_ShaderRemoved);

        assetManager.RegisterImportProduct(ImportProductDescriptor{
            .ProductId = "NativeCopy",
            .DisplayName = "Native Asset (copy)",
            .AcceptsSourceExtension = &AssetManager::AcceptsNativeCopyExtension,
            .bNeedsSkeletonPicker = false,
            .Import = &AssetManager::ImportProduct_NativeCopy});
        assetManager.RegisterImportProduct(ImportProductDescriptor{
            .ProductId = "StaticMesh",
            .DisplayName = "Static Mesh",
            .AcceptsSourceExtension = &AssetManager::AcceptsExternalMeshCookExtension,
            .bNeedsSkeletonPicker = false,
            .Import = &AssetManager::ImportProduct_StaticMesh});
        assetManager.RegisterImportProduct(ImportProductDescriptor{
            .ProductId = "SkeletalMesh",
            .DisplayName = "Skeletal Mesh",
            .AcceptsSourceExtension = &AssetManager::AcceptsExternalMeshCookExtension,
            .bNeedsSkeletonPicker = false,
            .Import = &AssetManager::ImportProduct_SkeletalMesh});
        assetManager.RegisterImportProduct(ImportProductDescriptor{
            .ProductId = "AnimationClip",
            .DisplayName = "Animation Clip",
            .AcceptsSourceExtension = &AssetManager::AcceptsAnimationClipSourceExtension,
            .bNeedsSkeletonPicker = true,
            .Import = &AssetManager::ImportProduct_AnimationClip});
    }
}
