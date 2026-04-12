#include "SceneSerializer.h"

#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"

#include <fstream>


namespace minEngine
{
    bool SceneSerializer::LoadScene(const std::filesystem::path& filePath, Scene& outScene)
    {
        
        return true;
    }

    bool SceneSerializer::SaveScene(const std::filesystem::path& filePath, const Scene& scene)
    {
        Serialization::JsonWriterArchive archive;
        Serialization::SerializerOptions options = {
            .enumAsString = true,
            .strictTypeCheck = true,
            .skipUnknownField = false,
            .allowObjectPtrSerialization = true
        };
        Json jsonScene;
        for(auto GO : scene.m_GameObjects)
        {
            Serialization::SerializeResult result = Serialization::Serializer::Serialize(
                "minEngine::GameObject",
                GO.second.get(),
                archive,
                options);
        }

        Json json = archive.GetRoot();
        std::ofstream outputFile(filePath);
        outputFile << json.dump(4);
        outputFile.close();
        return true;

    }

    
}