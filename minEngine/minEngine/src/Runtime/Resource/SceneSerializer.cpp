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
        std::ifstream inputFile(filePath);
        if (!inputFile.is_open())
        {
            ME_CORE_ERROR("Failed to open scene file '{}'", filePath.string());
            return false;
        }
        Json jsonScene;
        try
        {
            inputFile >> jsonScene;
        }
        catch (const std::exception& e)
        {
            ME_CORE_ERROR("Failed to parse scene file '{}': {}", filePath.string(), e.what());
            return false;
        }
        Serialization::JsonReaderArchive archive = Serialization::JsonReaderArchive(jsonScene);
        Serialization::SerializerOptions options = {
            .enumAsString = true,
            .strictTypeCheck = true,
            .skipUnknownField = false,
            .allowObjectPtrSerialization = true
        };
        std::shared_ptr<GameObject> tempGO = NewObject<GameObject>();
        Serialization::SerializeResult result = Serialization::Serializer::Deserialize(
            "minEngine::GameObject",
            tempGO.get(),
            archive,
            options);
        if(!result.ok)
        {
            ME_CORE_ERROR("Failed to deserialize GameObject from scene file '{}': {}", filePath.string(), result.message);
            return false;
        }

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
            // Serialize one GO currently
            archive.ResetRoot();
            Serialization::SerializeResult result = Serialization::Serializer::Serialize(
                "minEngine::GameObject",
                GO.second.get(),
                archive,
                options);
            if (!result.ok)            
            {
                ME_CORE_ERROR("Failed to serialize GameObject '{}': {}", GO.second->GetName(), result.message);
                // return false;
            }
        }

        Json firstGO = archive.GetRoot();
        std::ofstream outputFile(filePath);
        outputFile << firstGO.dump(4);
        outputFile.close();
        return true;

    }

    
}