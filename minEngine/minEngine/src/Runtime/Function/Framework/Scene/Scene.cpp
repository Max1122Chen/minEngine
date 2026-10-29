#include "Scene.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/GameplayFramework/Events/GameplayEventSystemComponent.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Core/Log/LogSystem.h"

#include <algorithm>
#include <functional>
#include <unordered_set>

namespace minEngine
{
    Scene::~Scene()
    {
        ME_LOG(LogCore, Info, "Scene '{}' is being destroyed. Cleaning up {} game objects.", m_SceneName, m_GameObjects.size());
        DeinitializeWorldSubsystems();

        // Removal before destruction. The components below cannot queue their own removal once the
        // GameObject they hang off is coming apart - each of them would find no owning render scene
        // and drop its proxy instead, leaving it registered on a scene that is about to die. The
        // Scene still knows both sides, so it drives the removal here.
        if (m_RenderScene && m_RenderScene->HasPendingProxyWork())
        {
            DetachSceneRenderState(*this);
        }

        m_GameplayEventSystem = nullptr;
        m_GameObjects.clear();
        m_GameObjectsById.clear();
        m_PrefabInstances.clear();

        // The removals are commands of the frame this Scene is released in, so the proxies they act
        // on are still owned here. A proxy may only be freed where that frame ended, which is also
        // where the removal is applied - so the render side is handed to that boundary instead of
        // being freed with the Scene.
        if (m_RenderScene && m_RenderScene->HasPendingProxyWork())
        {
            HandOverOrphanedRenderScene(std::move(m_RenderScene));
        }
        m_RenderScene.reset();
    }

    void Scene::InitializeWorldSubsystems()
    {
        if (m_WorldSubsystems.IsInitialized())
        {
            return;
        }

        ME_ASSERT(ObjectManager::HasInstance(), "InitializeWorldSubsystems requires ObjectManager");
        m_WorldSubsystems.Initialize(this);
    }

    void Scene::PostInitializeWorldSubsystems()
    {
        if (!m_WorldSubsystems.IsInitialized())
        {
            return;
        }

        m_WorldSubsystems.ForEachSubsystem([](Subsystem* subsystem) {
            WorldSubsystem* worldSubsystem = static_cast<WorldSubsystem*>(subsystem);
            if (worldSubsystem)
            {
                worldSubsystem->PostInitialize();
            }
        });
    }

    void Scene::DeinitializeWorldSubsystems()
    {
        if (m_WorldSubsystems.IsInitialized())
        {
            m_WorldSubsystems.Deinitialize();
        }
    }

    void Scene::TickWorldSubsystems(float deltaTime)
    {
        if (!m_WorldSubsystems.IsInitialized())
        {
            return;
        }

        m_WorldSubsystems.ForEachSubsystem([deltaTime](Subsystem* subsystem) {
            if (subsystem == nullptr || !subsystem->IsA(TickableWorldSubsystem::StaticClass()))
            {
                return;
            }

            TickableWorldSubsystem* tickable = static_cast<TickableWorldSubsystem*>(subsystem);
            if (tickable->IsTickable())
            {
                tickable->Tick(deltaTime);
            }
        });
    }

    void Scene::FixedTickWorldSubsystems(float fixedDeltaSeconds)
    {
        if (!m_WorldSubsystems.IsInitialized())
        {
            return;
        }

        m_WorldSubsystems.ForEachSubsystem([fixedDeltaSeconds](Subsystem* subsystem) {
            if (subsystem == nullptr || !subsystem->IsA(TickableWorldSubsystem::StaticClass()))
            {
                return;
            }

            TickableWorldSubsystem* tickable = static_cast<TickableWorldSubsystem*>(subsystem);
            if (tickable->WantsFixedTick())
            {
                tickable->FixedTick(fixedDeltaSeconds);
            }
        });
    }

    void Scene::FixedTickGameObjects(float fixedDeltaSeconds)
    {
        const std::vector<GameObject*> participants = m_FixedTickGameObjects;
        for (GameObject* gameObject : participants)
        {
            if (gameObject == nullptr || gameObject->IsPendingDestroy() ||
                !gameObject->IsActiveInHierarchy())
            {
                continue;
            }
            if (!gameObject->WantsFixedTick())
            {
                continue;
            }
            gameObject->FixedTick(fixedDeltaSeconds);
        }
    }

    void Scene::RegisterFixedTickGameObject(GameObject* gameObject)
    {
        if (gameObject == nullptr)
        {
            return;
        }
        if (std::find(m_FixedTickGameObjects.begin(), m_FixedTickGameObjects.end(), gameObject)
            != m_FixedTickGameObjects.end())
        {
            return;
        }
        m_FixedTickGameObjects.push_back(gameObject);
    }

    void Scene::UnregisterFixedTickGameObject(GameObject* gameObject)
    {
        if (gameObject == nullptr)
        {
            return;
        }
        m_FixedTickGameObjects.erase(
            std::remove(m_FixedTickGameObjects.begin(), m_FixedTickGameObjects.end(), gameObject),
            m_FixedTickGameObjects.end());
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
        StructuralTraversalScope scope(*this);
        const size_t count = m_GameObjects.size();
        for (size_t i = 0; i < count; ++i)
        {
            GameObject* gameObject = m_GameObjects[i].get();
            // An inactive node takes its whole subtree with it: every descendant is inactive too,
            // so the flat walk can stop at the node itself.
            if (gameObject == nullptr || gameObject->IsPendingDestroy() ||
                !gameObject->IsActiveInHierarchy())
            {
                continue;
            }

            gameObject->Tick(deltaTime);
        }
    }

    void Scene::DispatchBeginPlay()
    {
        if (m_bHasBegunPlay)
        {
            return;
        }

        // Set before the loop so SpawnGameObject during BeginPlay also BeginPlays.
        m_bHasBegunPlay = true;

        if (m_WorldSubsystems.IsInitialized())
        {
            m_WorldSubsystems.ForEachSubsystem([this](Subsystem* subsystem) {
                WorldSubsystem* worldSubsystem = static_cast<WorldSubsystem*>(subsystem);
                if (worldSubsystem)
                {
                    worldSubsystem->OnWorldBeginPlay(*this);
                }
            });
        }

        StructuralTraversalScope scope(*this);
        const size_t count = m_GameObjects.size();
        for (size_t i = 0; i < count; ++i)
        {
            GameObject* gameObject = m_GameObjects[i].get();
            if (gameObject == nullptr || gameObject->IsPendingDestroy())
            {
                continue;
            }

            gameObject->BeginPlay();
        }
    }

    void Scene::DispatchEndPlay(EEndPlayReason reason)
    {
        if (!m_bHasBegunPlay)
        {
            return;
        }

        StructuralTraversalScope scope(*this);
        const size_t count = m_GameObjects.size();
        for (size_t i = 0; i < count; ++i)
        {
            GameObject* gameObject = m_GameObjects[i].get();
            if (gameObject)
            {
                gameObject->EndPlay(reason);
            }
        }

        if (m_WorldSubsystems.IsInitialized())
        {
            m_WorldSubsystems.ForEachSubsystem([this, reason](Subsystem* subsystem) {
                WorldSubsystem* worldSubsystem = static_cast<WorldSubsystem*>(subsystem);
                if (worldSubsystem)
                {
                    worldSubsystem->OnWorldEndPlay(*this, reason);
                }
            });
        }

        m_bHasBegunPlay = false;
    }

    std::shared_ptr<GameObject> Scene::CreateGameObject()
    {
        return SpawnGameObject();
    }

    std::shared_ptr<GameObject> Scene::SpawnGameObject()
    {
        const uint64_t id = m_NextGOId++;
        auto gameObject = NewObject<GameObject>("", this);
        gameObject->SetID(id);
        m_GameObjects.push_back(gameObject);
        m_GameObjectsById[id] = gameObject.get();

        if (m_bHasBegunPlay)
        {
            gameObject->BeginPlay();
        }

        return gameObject;
    }

    void Scene::CollectSubtreeIds(GameObject* root, std::vector<uint64_t>& outIds) const
    {
        if (root == nullptr)
        {
            return;
        }

        for (GameObject* child : root->GetChildren())
        {
            CollectSubtreeIds(child, outIds);
        }

        outIds.push_back(root->GetID());
    }

    bool Scene::DestroyGameObject(uint64_t id)
    {
        GameObject* gameObject = FindGameObjectById(id);
        if (gameObject == nullptr)
        {
            return false;
        }

        if (gameObject->IsPendingDestroy())
        {
            return true;
        }

        std::vector<uint64_t> subtreeIds;
        CollectSubtreeIds(gameObject, subtreeIds);
        for (uint64_t destroyId : subtreeIds)
        {
            GameObject* node = FindGameObjectById(destroyId);
            if (node == nullptr || node->IsPendingDestroy())
            {
                continue;
            }

            node->MarkPendingDestroy();
            m_PendingDestroyIds.insert(destroyId);
        }

        return true;
    }

    bool Scene::IsPendingDestroy(uint64_t id) const
    {
        return m_PendingDestroyIds.find(id) != m_PendingDestroyIds.end();
    }

    void Scene::FlushDeferredDestroys()
    {
        if (m_PendingDestroyIds.empty())
        {
            return;
        }

        std::vector<uint64_t> pendingIds(m_PendingDestroyIds.begin(), m_PendingDestroyIds.end());
        m_PendingDestroyIds.clear();

        for (uint64_t pendingId : pendingIds)
        {
            if (FindGameObjectById(pendingId) == nullptr)
            {
                continue;
            }

            RemoveGameObjectById(pendingId);
        }
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
        DeinitializeWorldSubsystems();
        m_GameObjects.clear();
        m_GameObjectsById.clear();
        m_PendingDestroyIds.clear();
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
        if (m_StructuralTraversalDepth > 0
            || (SceneManager::HasInstance() && SceneManager::Get().IsInStructuralTraversal()))
        {
            ME_LOG(
                LogCore,
                Error,
                "RemoveGameObjectById({}) while structural traversal is active; use DestroyGameObject.",
                id);
            return false;
        }

        GameObject* gameObject = FindGameObjectById(id);
        if (!gameObject)
        {
            return false;
        }

        std::vector<uint64_t> toRemove;
        CollectSubtreeIds(gameObject, toRemove);

        for (uint64_t removeId : toRemove)
        {
            GameObject* node = FindGameObjectById(removeId);
            if (node == nullptr)
            {
                continue;
            }
            m_PendingDestroyIds.erase(removeId);
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
