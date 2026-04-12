#include "SceneSerializer.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

#include <fstream>

namespace minEngine
{
    bool SceneSerializer::LoadScene(const std::filesystem::path& filePath, Scene& outScene)
    {
        return true;
    }

    bool SceneSerializer::SaveScene(const std::filesystem::path& filePath, const Scene& scene)
    {
        return true;
    }

    
}