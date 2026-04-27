#include "Scene.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Core/Object/MEObject.h"

namespace minEngine
{
    void Scene::Tick(float deltaTime)
    {
        for (const std::shared_ptr<GameObject>& gameObject : m_GameObjects)
        {
            if (gameObject)
            {
                gameObject->Tick(deltaTime);
            }
        }
    }

    std::shared_ptr<GameObject> Scene::CreateGameObject()
    {
        const uint64_t id = m_NextGOId++;
        auto gameObject = NewObject<GameObject>("", this);
        gameObject->SetID(id);
        m_GameObjects.push_back(gameObject);
        m_GameObjectsById[id] = gameObject.get();
        return gameObject;
    }

    void Scene::Reset()
    {
        m_GameObjects.clear();
        m_GameObjectsById.clear();
        m_NextGOId = 0;
    }

    void Scene::RebuildRuntimeGameObjectIndex()
    {
        std::vector<std::shared_ptr<GameObject>> compactGameObjects;
        compactGameObjects.reserve(m_GameObjects.size());

        m_GameObjectsById.clear();
        m_NextGOId = 0;

        for (const std::shared_ptr<GameObject>& gameObject : m_GameObjects)
        {
            if (!gameObject)
            {
                continue;
            }

            gameObject->SetOuter(this);

            const uint64_t newId = m_NextGOId++;
            gameObject->SetID(newId);
            m_GameObjectsById[newId] = gameObject.get();

            compactGameObjects.push_back(gameObject);
        }

        m_GameObjects = std::move(compactGameObjects);
    }

    GameObject *Scene::FindGameObjectById(uint64_t id) const
    {
        const auto iter = m_GameObjectsById.find(id);
        if (iter == m_GameObjectsById.end())
        {
            return nullptr;
        }

        return iter->second;
    }
}