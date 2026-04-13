#include "Scene.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Core/Object/MEObject.h"

namespace minEngine
{
    void Scene::Tick(float deltaTime)
    {
        for (auto& [id, gameObject] : m_GameObjects)
        {
            gameObject->Tick(deltaTime);
        }
    }

    std::shared_ptr<GameObject> Scene::CreateGameObject()
    {
        const uint64_t id = m_NextObjectId++;
        auto gameObject = NewObject<GameObject>("", this);
        gameObject->m_ID = id;
        m_GameObjects[id] = gameObject;
        return gameObject;
    }

    std::shared_ptr<GameObject> Scene::CreateGameObject(uint64_t id)
    {
        auto gameObject = NewObject<GameObject>("", this);
        gameObject->m_ID = id;
        m_GameObjects[id] = gameObject;
        if (id >= m_NextObjectId)
        {
            m_NextObjectId = id + 1;
        }
        return gameObject;
    }
}