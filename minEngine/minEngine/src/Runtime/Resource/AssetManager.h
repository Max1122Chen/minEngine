#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Serialization/Json.h"
#include "AssetMeta.h"
#include "SceneSerializer.h"

#include <filesystem>
#include <fstream>

namespace minEngine
{
    class Texture2D;
    class StaticMesh;
    class Scene;

    class AssetManager
    {
    public:
        AssetManager() = default;
        ~AssetManager() = default;

        static AssetManager& Get();


        void Initialize();
        void Shutdown();

        void ScanAssets(const std::string& directory);
        void RegisterAsset(const std::string& path, const std::string& type);

        const AssetMeta* FindAssetMetaByPath(const std::string& path) const;
        const AssetMeta* FindAssetMetaByGuid(const GUID& guid) const;

        std::shared_ptr<StaticMesh> LoadStaticMeshByMeta(const AssetMeta& meta);
        std::shared_ptr<Texture2D> LoadTexture2DByMeta(const AssetMeta& meta, uint32_t unit);

        std::shared_ptr<StaticMesh> LoadStaticMeshByGuid(const GUID& guid);
        std::shared_ptr<Texture2D> LoadTexture2DByGuid(const GUID& guid, uint32_t unit);
    
        // Image loading using stb_image
        unsigned char* LoadImage(const std::string& path, int& width, int& height, int& channels, bool bFlip = true);
        void           FreeImage(unsigned char* data);

        // Static mesh loading
        std::shared_ptr<StaticMesh> LoadStaticMesh(const std::string& path);
        std::shared_ptr<Texture2D> LoadTexture2D(const std::string& path, uint32_t unit);

        std::shared_ptr<Scene> CreateNewScene(const std::string& sceneName);
        std::shared_ptr<Scene> LoadScene(const std::string& sceneName);

        template<typename T>
        bool LoadAsset(const std::string& path, T& asset) const
        {
            return true;
        }

        template<typename T>
        bool SaveAsset(const std::string& path, const T& asset) const
        {
            return true;
        }
        
    private:
        std::string NormalizeAssetPath(const std::string& path) const;
        std::string InferAssetType(const std::filesystem::path& path) const;
        std::filesystem::path BuildMetaPath(const std::filesystem::path& assetPath) const;
        bool LoadMetaFromDisk(const std::filesystem::path& metaPath, AssetMeta& outMeta) const;
        bool SaveMetaToDisk(const std::filesystem::path& metaPath, const AssetMeta& meta) const;
        void CacheMeta(const AssetMeta& meta);

        std::unordered_map<std::string, AssetMeta> m_AssetRegistry; // Maps asset paths to their metadata
        std::unordered_map<GUID, std::string, GUIDHasher> m_AssetPathByGuid;

        std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_LoadedTexture2DCache;
        std::unordered_map<std::string, std::shared_ptr<StaticMesh>> m_LoadedStaticMeshCache;
    };

    template<>
    inline bool AssetManager::LoadAsset<Scene>(const std::string& path, Scene& asset) const
    {
        std::filesystem::path assetPath(path);
        return SceneSerializer::LoadScene(assetPath, asset);
    }

    template<>
    inline bool AssetManager::SaveAsset<Scene>(const std::string& path, const Scene& asset) const
    {
        std::filesystem::path assetPath(path);
        return SceneSerializer::SaveScene(assetPath, asset);
    }

    
}