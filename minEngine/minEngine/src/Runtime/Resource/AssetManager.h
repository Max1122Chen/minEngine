#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"

namespace minEngine
{
    class Texture2D;
    class StaticMesh;

    class AssetManager
    {
    public:
        AssetManager() = default;
        ~AssetManager() = default;

        static AssetManager& GetAssetManager();


        void Initialize() {}
        void Shutdown() {}

        // Image loading using stb_image
        unsigned char* LoadImage(const std::string& path, int& width, int& height, int& channels, bool bFlip = true);
        void           FreeImage(unsigned char* data);

        // Static mesh loading using 
        void LoadStaticMesh(const std::string& path, StaticMesh* outMesh);
        
    private:
        std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_LoadedTexture2DCache;
        std::unordered_map<std::string, std::shared_ptr<StaticMesh>> m_LoadedStaticMeshCache;
    };
}