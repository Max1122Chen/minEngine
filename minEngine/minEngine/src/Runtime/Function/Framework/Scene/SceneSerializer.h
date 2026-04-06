#pragma once

#include "Core.h"
#include "Runtime/Core/Serialization/Serializer.h"

namespace minEngine
{
    class Scene;

    class SceneSerializer
    {
    public:
        static bool SaveScene(const Scene& scene, const std::filesystem::path& filePath);
        static bool LoadScene(const std::filesystem::path& filePath, Scene& outScene);
    };
}
