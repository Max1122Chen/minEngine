#include "SceneSerializer.h"

#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

#include <fstream>

namespace minEngine
{
    namespace
    {
        Json WriteTransformJson(const Transform& transform)
        {
            Json result = Json::object();
            result["position"] = Serializer::Write<Vector3>(transform.Position);
            result["rotation"] = Serializer::Write<Vector3>(transform.Rotation);
            result["scale"] = Serializer::Write<Vector3>(transform.Scale);
            return result;
        }

        bool ReadTransformJson(const Json& json, Transform& outTransform)
        {
            if (!json.is_object())
            {
                return false;
            }

            if (json.contains("position"))
            {
                Serializer::Read<Vector3>(json["position"], outTransform.Position);
            }
            if (json.contains("rotation"))
            {
                Serializer::Read<Vector3>(json["rotation"], outTransform.Rotation);
            }
            if (json.contains("scale"))
            {
                Serializer::Read<Vector3>(json["scale"], outTransform.Scale);
            }

            return true;
        }
    }

    bool SceneSerializer::LoadScene(const std::filesystem::path& filePath, Scene& outScene)
    {
        std::ifstream input(filePath);
        if (!input.is_open())
        {
            ME_CORE_ERROR("[SceneSerializer] Failed to open file '{}'", filePath.string());
            return false;
        }

        Json root;
        try
        {
            input >> root;
        }
        catch (const std::exception& e)
        {
            ME_CORE_ERROR("[SceneSerializer] Failed to parse json file '{}': {}", filePath.string(), e.what());
            return false;
        }

        if (!root.is_object())
        {
            ME_CORE_ERROR("[SceneSerializer] Root json is not an object in '{}'", filePath.string());
            return false;
        }

        outScene.m_GameObjects.clear();
        outScene.sceneName = root.value("sceneName", std::string("Scene"));

        if (!root.contains("gameObjects") || !root["gameObjects"].is_array())
        {
            return true;
        }

        for (const Json& objectJson : root["gameObjects"])
        {
            if (!objectJson.is_object())
            {
                continue;
            }

            const uint64_t objectId = objectJson.value("id", 0ull);
            std::shared_ptr<GameObject> gameObject = outScene.CreateGameObject(objectId);
            if (!gameObject)
            {
                continue;
            }

            if (!gameObject->GetRootComponent())
            {
                std::shared_ptr<SceneComponent> rootComponent = std::make_shared<SceneComponent>();
                rootComponent->SetOwner(gameObject.get());
                gameObject->SetRootComponent(rootComponent);
                gameObject->GetComponents().push_back(rootComponent);
            }

            gameObject->SetName(objectJson.value("name", std::string("GameObject_") + std::to_string(objectId)));

            if (objectJson.contains("transform") && gameObject->GetRootComponent())
            {
                Transform transform = gameObject->GetRootComponent()->GetTransform();
                if (ReadTransformJson(objectJson["transform"], transform))
                {
                    gameObject->SetTransform(transform);
                }
            }

            if (!objectJson.contains("components") || !objectJson["components"].is_array())
            {
                continue;
            }

            for (const Json& componentJson : objectJson["components"])
            {
                if (!componentJson.is_object() || !componentJson.contains("$typeName") || !componentJson["$typeName"].is_string())
                {
                    continue;
                }

                const std::string typeName = componentJson["$typeName"].get<std::string>();
                std::shared_ptr<Component> component = Reflection::CreateInstanceAs<Component>(typeName);
                if (!component)
                {
                    ME_CORE_WARN("[SceneSerializer] Failed to instantiate component type '{}'", typeName);
                    continue;
                }

                if (!Serializer::ReadByName(typeName, componentJson, component.get()))
                {
                    ME_CORE_WARN("[SceneSerializer] Failed to deserialize component type '{}'", typeName);
                    continue;
                }

                component->SetOwner(gameObject.get());
                if (!gameObject->GetRootComponent())
                {
                    if (std::shared_ptr<SceneComponent> sceneComponent = std::dynamic_pointer_cast<SceneComponent>(component))
                    {
                        gameObject->SetRootComponent(sceneComponent);
                    }
                }
                gameObject->GetComponents().push_back(component);
            }
        }

        return true;
    }

    bool SceneSerializer::SaveScene(const std::filesystem::path& filePath, const Scene& scene)
    {
        Json root = Json::object();
        root["sceneVersion"] = 1;
        root["sceneName"] = scene.sceneName;

        Json gameObjects = Json::array();
        for (const auto& [id, gameObject] : scene.m_GameObjects)
        {
            if (!gameObject)
            {
                continue;
            }

            Json objectJson = Json::object();
            objectJson["id"] = id;
            objectJson["name"] = gameObject->GetName();

            if (gameObject->GetRootComponent())
            {
                objectJson["transform"] = WriteTransformJson(gameObject->GetRootComponent()->GetTransform());
            }

            Json components = Json::array();
            for (const std::shared_ptr<Component>& component : gameObject->GetComponents())
            {
                if (!component)
                {
                    continue;
                }

                const std::string dynamicTypeName = Reflection::GetDeclaredTypeNameByTypeId(typeid(*component).name());
                if (dynamicTypeName.empty())
                {
                    ME_CORE_WARN("[SceneSerializer] Missing reflection type for component on object {}", id);
                    continue;
                }

                components.push_back(Serializer::WriteByName(dynamicTypeName, component.get()));
            }

            objectJson["components"] = std::move(components);
            gameObjects.push_back(std::move(objectJson));
        }

        root["gameObjects"] = std::move(gameObjects);

        std::error_code ec;
        const std::filesystem::path parentPath = filePath.parent_path();
        if (!parentPath.empty())
        {
            std::filesystem::create_directories(parentPath, ec);
            if (ec)
            {
                ME_CORE_ERROR("[SceneSerializer] Failed to create directory '{}': {}", parentPath.string(), ec.message());
                return false;
            }
        }

        std::ofstream output(filePath, std::ios::out | std::ios::trunc);
        if (!output.is_open())
        {
            ME_CORE_ERROR("[SceneSerializer] Failed to open file '{}' for write", filePath.string());
            return false;
        }

        output << root.dump(4);
        output.flush();
        return output.good();
    }

    
}