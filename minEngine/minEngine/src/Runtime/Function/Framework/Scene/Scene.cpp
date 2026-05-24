#include "Scene.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Render/RenderScene.h"

namespace minEngine
{
    Scene::~Scene()
    {
        ME_CORE_INFO("Scene '{}' is being destroyed. Cleaning up {} game objects.", m_SceneName, m_GameObjects.size());
        m_GameObjects.clear();
        m_GameObjectsById.clear();
        m_RenderScene.reset();
    }

    void Scene::MarkReachableObjects(const std::function<void(MEObject*)>& markReachable) const
    {
        markReachable(const_cast<Scene*>(this));
        for (const std::shared_ptr<GameObject>& gameObject : m_GameObjects)
        {
            if (!gameObject)
            {
                continue;
            }

            markReachable(gameObject.get());
            for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
            {
                if (component)
                {
                    markReachable(component.get());
                }
            }
        }
    }

    void Scene::EnsureRenderScene()
    {
        if (!m_RenderScene)
        {
            m_RenderScene = std::make_shared<RenderScene>();
        }
    }

    RenderScene* Scene::GetRenderScene()
    {
        EnsureRenderScene();
        return m_RenderScene.get();
    }

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

    bool Scene::RemoveGameObjectById(uint64_t id)
    {
        GameObject* gameObject = FindGameObjectById(id);
        if (!gameObject)
        {
            return false;
        }

        m_GameObjects.erase(std::remove_if(m_GameObjects.begin(), m_GameObjects.end(), [gameObject](const std::shared_ptr<GameObject>& go) {
            return go->GetID() == gameObject->GetID();
        }), m_GameObjects.end());
        m_GameObjectsById.erase(id);
        return true;
    }
}