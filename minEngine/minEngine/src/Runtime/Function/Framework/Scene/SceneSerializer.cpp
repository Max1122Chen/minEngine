#include "SceneSerializer.h"

#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/Component.h"

namespace minEngine
{
    bool SceneSerializer::SaveScene(const Scene& scene, const std::filesystem::path& filePath)
    {
        Serializer::Json root = Serializer::Json::object();
        root["version"] = 1;
        root["sceneName"] = scene.sceneName;

        Serializer::Json gameObjects = Serializer::Json::array();
        for (const auto& [id, gameObject] : scene.m_GameObjects)
        {
            if (!gameObject)
            {
                continue;
            }

            Serializer::Json objectJson = Serializer::Json::object();
            objectJson["id"] = id;
            objectJson["name"] = gameObject->GetName();

            if (const std::shared_ptr<SceneComponent> rootComponent = gameObject->GetRootComponent())
            {
                objectJson["transform"] = Serializer::ToJson(rootComponent->GetTransform());
            }

            Serializer::Json componentsJson = Serializer::Json::array();
            for (const std::shared_ptr<Component>& component : gameObject->GetComponents())
            {
                if (!component)
                {
                    continue;
                }

                const Reflection::TypeInfo* componentTypeInfo = Reflection::ReflectionSystem::Get().GetTypeInfoByTypeId(typeid(*component).name());
                if (componentTypeInfo == nullptr)
                {
                    ME_CORE_WARN("[SceneSerializer] Missing reflection type info for component on game object {}.", id);
                    continue;
                }

                Serializer::Json componentJson = Serializer::Json::object();
                componentJson["data"] = Serializer::ToJsonByTypeInfo(component.get(), *componentTypeInfo);
                componentJson["type"] = componentTypeInfo->name;
                componentsJson.push_back(std::move(componentJson));
            }

            objectJson["components"] = std::move(componentsJson);
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
                ME_CORE_ERROR("[SceneSerializer] Failed to create scene directory '{}': {}", parentPath.string(), ec.message());
                return false;
            }
        }

        return Serializer::SaveJsonToFile(root, filePath);
    }

    bool SceneSerializer::LoadScene(const std::filesystem::path& filePath, Scene& outScene)
    {
        Serializer::Json root;
        if (!Serializer::LoadJsonFromFile(filePath, root))
        {
            return false;
        }

        if (!root.is_object())
        {
            ME_CORE_ERROR("[SceneSerializer] Scene file '{}' root is not an object.", filePath.string());
            return false;
        }

        outScene.m_GameObjects.clear();
        outScene.sceneName = root.value("sceneName", filePath.string());

        if (!root.contains("gameObjects") || !root["gameObjects"].is_array())
        {
            return true;
        }

        for (const auto& objectJson : root["gameObjects"])
        {
            if (!objectJson.is_object())
            {
                continue;
            }

            const uint64_t id = objectJson.value("id", 0ull);
            std::shared_ptr<GameObject> gameObject = outScene.CreateGameObject(id);
            gameObject->SetName(objectJson.value("name", std::string("GameObject_") + std::to_string(id)));

            if (objectJson.contains("transform") && objectJson["transform"].is_object() && gameObject->GetRootComponent())
            {
                Transform transform = gameObject->GetRootComponent()->GetTransform();
                if (!Serializer::FromJson(objectJson["transform"], transform))
                {
                    ME_CORE_WARN("[SceneSerializer] Failed to deserialize transform for game object {}.", id);
                }
                gameObject->SetTransform(transform);
            }

            if (objectJson.contains("components") && objectJson["components"].is_array())
            {
                for (const Serializer::Json& componentJson : objectJson["components"])
                {
                    if (!componentJson.is_object() || !componentJson.contains("type") || !componentJson["type"].is_string())
                    {
                        continue;
                    }

                    const std::string typeName = componentJson["type"].get<std::string>();
                    const Reflection::TypeInfo* componentTypeInfo = Reflection::ReflectionSystem::Get().GetTypeInfo(typeName);
                    if (componentTypeInfo == nullptr)
                    {
                        ME_CORE_WARN("[SceneSerializer] Unknown component type '{}'", typeName);
                        continue;
                    }

                    std::shared_ptr<Component> component = Reflection::ReflectionSystem::Get().CreateInstanceAs<Component>(typeName);
                    if (!component)
                    {
                        ME_CORE_WARN("[SceneSerializer] Failed to instantiate component type '{}'", typeName);
                        continue;
                    }

                    if (componentJson.contains("data") && componentJson["data"].is_object())
                    {
                        if (!Serializer::FromJsonByTypeInfo(componentJson["data"], component.get(), *componentTypeInfo))
                        {
                            ME_CORE_WARN("[SceneSerializer] Failed to deserialize component data for type '{}'", typeName);
                        }
                    }

                    component->SetOwner(gameObject.get());
                    gameObject->GetComponents().push_back(component);
                }
            }
        }

        return true;
    }
}
