#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Serialization/Json.h"
#include "Runtime/Core/Serialization/Serializer.h"

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

        static AssetManager& GetAssetManager();


        void Initialize() {}
        void Shutdown();
    
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
            // TODO: maybe we need to support relative path
            std::filesystem::path assetPath(path);
            std::ifstream assetJsonFile(assetPath);
            if (!assetJsonFile)
            {
                ME_CORE_ERROR("Failed to open asset file: {}", assetPath.string());
                return false;
            }

            std::stringstream buffer;
            buffer << assetJsonFile.rdbuf();
            assetJsonFile.close();

            std::string assetJsonText = buffer.str();

            // Parse to json object
            std::string errorMessage;
            Json assetJson;
            try
            {
                assetJson = Json::parse(assetJsonText);
            }
            catch (const std::exception& e)
            {
                ME_CORE_ERROR("Failed to parse asset json file '{}'. Error: {}", assetPath.string(), e.what());
                return false;
            }

            Serializer::Read(assetJson, asset);
            return true;
        }

        template<typename T>
        bool SaveAsset(const std::string& path, const T& asset) const
        {
            // TODO: maybe we need to support relative path
            std::filesystem::path assetPath(path);
            std::ofstream assetJsonFile(assetPath);

            if (!assetJsonFile)
            {
                ME_CORE_ERROR("Failed to open asset file for writing: {}", assetPath.string());
                return false;
            }

            Json&& assetJson;
            assetJson = Serializer::Write(asset);
            assetJsonFile << assetJson.dump(4);
            assetJsonFile.flush();
            assetJsonFile.close();
            return true;
        }
        
    private:
        std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_LoadedTexture2DCache;
        std::unordered_map<std::string, std::shared_ptr<StaticMesh>> m_LoadedStaticMeshCache;
    };

    
}