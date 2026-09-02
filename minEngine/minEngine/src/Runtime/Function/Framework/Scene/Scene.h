#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Function/Physics/PhysicsTypes.h"
#include "Runtime/Resource/Asset.h"

#include <functional>

namespace minEngine
{
    class GameObject;
    class RenderScene;

    ME_CLASS()
    class Scene : public Asset
    {  
        ME_GENERATED_BODY(Scene)
    public:
        Scene() = default;
        virtual ~Scene();

        void Tick(float deltaTime);

        std::shared_ptr<GameObject> CreateGameObject();
        std::shared_ptr<GameObject> InsertRestoredGameObject(std::shared_ptr<GameObject> gameObject);
        void Reset();
        void RebuildRuntimeGameObjectIndex();
        const std::string& GetSceneName() const { return m_SceneName; }
        const std::vector<std::shared_ptr<GameObject>>& GetAllGameObjects() const { return m_GameObjects; }
        const std::unordered_map<uint64_t, GameObject*>& GetGameObjectsById() const { return m_GameObjectsById; }
        GameObject* FindGameObjectById(uint64_t id) const;
        bool RemoveGameObjectById(uint64_t id);
        uint64_t IncrementNextGOId() { return m_NextGOId++; }

        /**
         * Closest ray query against this scene's physics world (UE UWorld-style entry point).
         * TraceChannel uses the same ECollisionChannel enum as ObjectChannel (Trace usage).
         */
        bool LineTrace(
            const Vector3& start,
            const Vector3& end,
            ECollisionChannel traceChannel,
            const CollisionQueryParams& params,
            HitResult& outHit);

        bool SphereTrace(
            const Vector3& start,
            const Vector3& end,
            float radius,
            ECollisionChannel traceChannel,
            const CollisionQueryParams& params,
            HitResult& outHit);

        bool CapsuleTrace(
            const Vector3& start,
            const Vector3& end,
            float radius,
            float halfHeight,
            ECollisionChannel traceChannel,
            const CollisionQueryParams& params,
            HitResult& outHit);

        void EnsureRenderScene();
        RenderScene* GetRenderScene();
        const std::shared_ptr<RenderScene>& GetRenderSceneShared() const { return m_RenderScene; }

        ESceneType GetSceneType() const { return m_SceneType; }
        void SetSceneType(ESceneType sceneType) { m_SceneType = sceneType; }
        ESceneTickPolicy GetTickPolicy() const { return m_TickPolicy; }
        void SetTickPolicy(ESceneTickPolicy tickPolicy) { m_TickPolicy = tickPolicy; }
        int32_t GetPIEInstanceId() const { return m_PIEInstanceId; }
        void SetPIEInstanceId(int32_t instanceId) { m_PIEInstanceId = instanceId; }
        bool IsEditorScene() const { return m_SceneType == ESceneType::Editor; }
        bool IsPIEScene() const { return m_SceneType == ESceneType::PIE; }

        void MarkReachableObjects(const std::function<void(MEObject*)>& markReachable) const;

    // private: // temporarily public for testing
        ME_PROPERTY()
        std::string m_SceneName;

        ME_PROPERTY(Instanced)
        std::vector<std::shared_ptr<GameObject>> m_GameObjects;

        std::unordered_map<uint64_t, GameObject*> m_GameObjectsById;

    private:
        uint64_t m_NextGOId{ 0 };
        std::shared_ptr<RenderScene> m_RenderScene;
        ESceneType m_SceneType{ ESceneType::None };
        ESceneTickPolicy m_TickPolicy{ ESceneTickPolicy::Gameplay };
        int32_t m_PIEInstanceId{ -1 };
    };
}

#include "Scene.gen.h"
