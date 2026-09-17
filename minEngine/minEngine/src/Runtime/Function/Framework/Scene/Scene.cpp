#include "Scene.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"
#include "Runtime/Function/GameplayFramework/Events/GameplayEventSystemComponent.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Core/Log/LogSystem.h"

#include <algorithm>
#include <functional>

namespace minEngine
{
    Scene::~Scene()
    {
        ME_LOG(LogCore, Info, "Scene '{}' is being destroyed. Cleaning up {} game objects.", m_SceneName, m_GameObjects.size());
        m_GameplayEventSystem = nullptr;
        m_GameObjects.clear();
        m_GameObjectsById.clear();
        m_PrefabInstances.clear();
        m_RenderScene.reset();
    }

    void Scene::RegisterGameplayEventSystem(GameplayEventSystemComponent* component)
    {
        if (component == nullptr)
        {
            return;
        }

        if (m_GameplayEventSystem != nullptr && m_GameplayEventSystem != component)
        {
            ME_LOG(LogCore, Warn, 
                "Scene '{}': multiple GameplayEventSystemComponent instances; keeping the first registered.",
                m_SceneName);
            return;
        }

        m_GameplayEventSystem = component;
    }

    void Scene::UnregisterGameplayEventSystem(GameplayEventSystemComponent* component)
    {
        if (m_GameplayEventSystem == component)
        {
            m_GameplayEventSystem = nullptr;
        }
    }

    bool Scene::LineTrace(
        const Vector3& start,
        const Vector3& end,
        ECollisionChannel traceChannel,
        const CollisionQueryParams& params,
        HitResult& outHit)
    {
        outHit = HitResult{};
        if (!PhysicsSystem::HasInstance())
        {
            return false;
        }

        return PhysicsSystem::Get().GetOrCreateWorld(this).LineTrace(
            start,
            end,
            traceChannel,
            params,
            outHit);
    }

    bool Scene::SphereTrace(
        const Vector3& start,
        const Vector3& end,
        float radius,
        ECollisionChannel traceChannel,
        const CollisionQueryParams& params,
        HitResult& outHit)
    {
        outHit = HitResult{};
        if (!PhysicsSystem::HasInstance())
        {
            return false;
        }

        return PhysicsSystem::Get().GetOrCreateWorld(this).SphereTrace(
            start,
            end,
            radius,
            traceChannel,
            params,
            outHit);
    }

    bool Scene::CapsuleTrace(
        const Vector3& start,
        const Vector3& end,
        float radius,
        float halfHeight,
        ECollisionChannel traceChannel,
        const CollisionQueryParams& params,
        HitResult& outHit)
    {
        outHit = HitResult{};
        if (!PhysicsSystem::HasInstance())
        {
            return false;
        }

        return PhysicsSystem::Get().GetOrCreateWorld(this).CapsuleTrace(
            start,
            end,
            radius,
            halfHeight,
            traceChannel,
            params,
            outHit);
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

    std::shared_ptr<GameObject> Scene::Instantiate(
        const Prefab& prefab,
        const PrefabInstantiateParams& params,
        std::string* outError)
    {
        return PrefabUtility::Instantiate(prefab, *this, params, outError);
    }

    std::shared_ptr<GameObject> Scene::InsertRestoredGameObject(std::shared_ptr<GameObject> gameObject)
    {
        if (!gameObject)
        {
            return nullptr;
        }

        const uint64_t id = m_NextGOId++;
        gameObject->SetID(id);
        gameObject->SetOuter(this);
        m_GameObjects.push_back(gameObject);
        m_GameObjectsById[id] = gameObject.get();
        return gameObject;
    }

    void Scene::Reset()
    {
        m_GameObjects.clear();
        m_GameObjectsById.clear();
        m_PrefabInstances.clear();
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

        std::vector<uint64_t> toRemove;
        std::function<void(GameObject*)> collectSubtree = [&](GameObject* node)
        {
            if (node == nullptr)
            {
                return;
            }
            for (GameObject* child : node->GetChildren())
            {
                collectSubtree(child);
            }
            toRemove.push_back(node->GetID());
        };
        collectSubtree(gameObject);

        for (uint64_t removeId : toRemove)
        {
            GameObject* node = FindGameObjectById(removeId);
            if (node == nullptr)
            {
                continue;
            }
            node->DetachFromParent(AttachmentTransformRules::KeepWorldTransform);
            m_GameObjects.erase(
                std::remove_if(
                    m_GameObjects.begin(),
                    m_GameObjects.end(),
                    [removeId](const std::shared_ptr<GameObject>& go)
                    {
                        return go && go->GetID() == removeId;
                    }),
                m_GameObjects.end());
            m_GameObjectsById.erase(removeId);
        }

        m_PrefabInstances.erase(
            std::remove_if(
                m_PrefabInstances.begin(),
                m_PrefabInstances.end(),
                [this](const PrefabInstanceRecord& record)
                {
                    for (const std::shared_ptr<GameObject>& go : m_GameObjects)
                    {
                        if (go && go->GetGuid() == record.RootInstanceGuid)
                        {
                            return false;
                        }
                    }
                    return true;
                }),
            m_PrefabInstances.end());

        return true;
    }

    void Scene::ResolveGameObjectHierarchy()
    {
        for (const std::shared_ptr<GameObject>& gameObject : m_GameObjects)
        {
            if (gameObject)
            {
                gameObject->ClearChildrenLinks();
            }
        }

        for (const std::shared_ptr<GameObject>& gameObject : m_GameObjects)
        {
            if (!gameObject)
            {
                continue;
            }

            GameObject* parent = gameObject->GetParent();
            if (parent == nullptr)
            {
                continue;
            }

            bool parentInScene = false;
            for (const std::shared_ptr<GameObject>& candidate : m_GameObjects)
            {
                if (candidate.get() == parent)
                {
                    parentInScene = true;
                    break;
                }
            }

            if (!parentInScene)
            {
                ME_LOG(LogCore, Error, 
                    "Scene::ResolveGameObjectHierarchy: GO '{}' parent not in scene; becoming root.",
                    gameObject->GetName());
                gameObject->DetachFromParent(AttachmentTransformRules::KeepRelativeTransform);
                continue;
            }

            if (!gameObject->AttachToParent(parent, AttachmentTransformRules::KeepRelativeTransform))
            {
                ME_LOG(LogCore, Error, 
                    "Scene::ResolveGameObjectHierarchy: failed to attach '{}'; becoming root.",
                    gameObject->GetName());
                gameObject->DetachFromParent(AttachmentTransformRules::KeepRelativeTransform);
            }
        }
    }
}
