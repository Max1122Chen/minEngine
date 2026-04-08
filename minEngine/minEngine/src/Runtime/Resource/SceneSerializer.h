#pragma once
#include "Core.h"

namespace minEngine
{
    class Scene;

    class SceneSerializer
    {
    public:
        static bool LoadScene(const std::filesystem::path& filePath, Scene& outScene);
        static bool SaveScene(const std::filesystem::path& filePath, const Scene& scene);
    };
}