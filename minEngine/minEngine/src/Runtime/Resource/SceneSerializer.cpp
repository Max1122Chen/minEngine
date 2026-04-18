#include "SceneSerializer.h"

#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

#include <fstream>

namespace minEngine
{
    namespace
    {
        constexpr const char* kSceneVersionField = "version";
        constexpr const char* kSceneDataField = "sceneData";
        constexpr int kSceneFileVersion = 2;
        constexpr const char* kSceneClassName = "minEngine::Scene";
    }

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

        if (!jsonScene.is_object())
        {
            ME_CORE_ERROR("Invalid scene json root for '{}': root is not an object.", filePath.string());
            return false;
        }

        if (!jsonScene.contains(kSceneDataField) || !jsonScene[kSceneDataField].is_object())
        {
            ME_CORE_ERROR("Invalid scene json for '{}': missing '{}' object.", filePath.string(), kSceneDataField);
            return false;
        }

        Serialization::SerializerOptions options = {
            .enumAsString = true,
            .strictTypeCheck = true,
            .skipUnknownField = false,
            .allowObjectPtrSerialization = true
        };

        Serialization::Serializer::ClearPendingObjectRefs();
        outScene.Reset();
        outScene.sceneName = filePath.string();

        Serialization::JsonReaderArchive archive(jsonScene[kSceneDataField]);
        Serialization::SerializeResult deserializeResult = Serialization::Serializer::Deserialize(
            kSceneClassName,
            &outScene,
            archive,
            options);
        if (!deserializeResult.ok)
        {
            ME_CORE_ERROR("Failed to deserialize Scene from '{}': {}", filePath.string(), deserializeResult.message);
            Serialization::Serializer::ClearPendingObjectRefs();
            return false;
        }

        outScene.RebuildRuntimeGameObjectIndex();

        Serialization::SerializeResult resolveResult = Serialization::Serializer::ResolvePendingObjectRefs();
        if (!resolveResult.ok)
        {
            ME_CORE_ERROR("Failed to resolve pending scene references for '{}'. pendingCount={}, reason={}",
                          filePath.string(),
                          Serialization::Serializer::GetPendingObjectRefCount(),
                          resolveResult.message);
            Serialization::Serializer::ClearPendingObjectRefs();
            return false;
        }

        if (outScene.sceneName.empty())
        {
            outScene.sceneName = filePath.string();
        }

        Serialization::Serializer::ClearPendingObjectRefs();

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

        Serialization::SerializeResult serializeResult = Serialization::Serializer::Serialize(
            kSceneClassName,
            &scene,
            archive,
            options);
        if (!serializeResult.ok)
        {
            ME_CORE_ERROR("Failed to serialize Scene '{}': {}", filePath.string(), serializeResult.message);
            return false;
        }

        Json jsonScene = Json::object();
        jsonScene[kSceneVersionField] = kSceneFileVersion;
        jsonScene[kSceneDataField] = archive.GetRoot();

        std::ofstream outputFile(filePath);
        if (!outputFile.is_open())
        {
            ME_CORE_ERROR("Failed to open output scene file '{}'.", filePath.string());
            return false;
        }

        outputFile << jsonScene.dump(4);
        if (!outputFile.good())
        {
            ME_CORE_ERROR("Failed to write scene file '{}'.", filePath.string());
            return false;
        }

        return true;
    }
}