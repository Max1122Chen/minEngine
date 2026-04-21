#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "AssetMeta.h"

#include <filesystem>

namespace minEngine
{
    class MEObject;
    class Texture2D;
    class StaticMesh;
    class Material;
    class Shader;
    class Scene;

    class AssetManager
    {
    public:
        AssetManager() = default;
        ~AssetManager() = default;

        static AssetManager& Get();


        void Initialize();
        void Shutdown();

        void ScanAssets(const std::filesystem::path& directory);
        AssetMeta RegisterAsset(const std::string& path, const std::string& type);

        std::shared_ptr<void> LoadAssetByGUID(const GUID& guid, std::string& outErrorMessage);

        const AssetMeta* FindAssetMetaByPath(const std::string& path) const;
        const AssetMeta* FindAssetMetaByGuid(const GUID& guid) const;
    
        template<typename T>
        std::shared_ptr<T> LoadAsset(const std::string& path)
        {
            // TODO: check the cache first before loading from disk, and populate the cache after loading
            
            // TODO: then check if the assetmeta exists and matches the expected type, to fail faster if the caller is trying to load an asset with the wrong type

            const AssetMeta* meta = FindAssetMetaByPath(path);
            if (meta == nullptr)
            {
                return nullptr;
            }

            return LoadAsset_Impl<T>(*meta);
        }

        template<typename T>
        bool SaveAsset(const std::string& path, const T& asset) const
        {
            const AssetMeta* meta = FindAssetMetaByPath(path);
            if (meta == nullptr)
            {
                return false;
            }

            return SaveAsset_Impl<T>(*meta, asset);
        }
        
    private:
        template<typename T>
        struct AlwaysFalse
        {
            static constexpr bool value = false;
        };

        template<typename T>
        std::shared_ptr<T> CreateAsset(const std::string& name, const std::string& directory)
        {
            static_assert(AlwaysFalse<T>::value, "CreateAsset<T> is not implemented for this type T");
            (void)name;
            (void)directory;
            return nullptr;
        }

        template<typename T>
        std::shared_ptr<T> RemoveAsset(const std::string& path)
        {
            static_assert(AlwaysFalse<T>::value, "RemoveAsset<T> is not implemented for this type T");
            (void)path;
            return nullptr;
        }

        template<typename T>
        std::shared_ptr<T> LoadAsset_Impl(const AssetMeta& meta)
        {
            static_assert(AlwaysFalse<T>::value, "LoadAsset_Impl<T> is not implemented for this type T");
            (void)meta;
            return nullptr;
        }

        template<typename T>
        bool SaveAsset_Impl(const AssetMeta& meta, const T& asset) const
        {
            static_assert(AlwaysFalse<T>::value, "SaveAsset_Impl<T> is not implemented for this type T");
            (void)meta;
            (void)asset;
            return true;
        }

        // Image loading using stb_image
        unsigned char* LoadImage(const std::string& path, int& width, int& height, int& channels, bool bFlip = true);
        void           FreeImage(unsigned char* data);

    private:
        std::string NormalizeAssetPath(const std::string& path) const;
        std::string InferAssetType(const std::filesystem::path& path) const;
        std::filesystem::path BuildMetaPath(const std::filesystem::path& assetPath) const;
        void CacheMeta(const AssetMeta& meta);

        std::unordered_map<std::string, AssetMeta> m_AssetRegistry; // Maps asset paths to their metadata
        std::unordered_map<GUID, std::string, GUID::Hash> m_AssetPathByGuid;

        std::unordered_map<std::string, std::weak_ptr<MEObject>> m_LoadedAssetCache; // Generic cache for loaded assets by path
    };
    
    // Load Asset Impl specializations
    template<>
    std::shared_ptr<Scene> AssetManager::LoadAsset_Impl<Scene>(const AssetMeta& meta);
    template<>
    std::shared_ptr<StaticMesh> AssetManager::LoadAsset_Impl<StaticMesh>(const AssetMeta& meta);
    template<>
    std::shared_ptr<Texture2D> AssetManager::LoadAsset_Impl<Texture2D>(const AssetMeta& meta);
    template<>
    std::shared_ptr<Material> AssetManager::LoadAsset_Impl<Material>(const AssetMeta& meta);
    template<>
    std::shared_ptr<Shader> AssetManager::LoadAsset_Impl<Shader>(const AssetMeta& meta);


    // Save Asset Impl specializations
    template<>
    bool AssetManager::SaveAsset_Impl<Scene>(const AssetMeta& meta, const Scene& asset) const;
}