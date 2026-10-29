#include "PhysicsDebugDraw.h"

#include "BoxColliderComponent.h"
#include "CapsuleColliderComponent.h"
#include "ColliderComponent.h"
#include "SphereColliderComponent.h"

#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <atomic>

namespace minEngine::PhysicsDebugDraw
{
    namespace
    {
        // Plain counters: the reading and the reading-back can happen on different roles, and a
        // caller that only wants "did this interval read anything" must not need a lock.
        std::atomic<uint64_t> s_SceneWalks{0};
        std::atomic<uint64_t> s_GameObjectReads{0};
        std::atomic<uint64_t> s_ColliderReads{0};

        void CountSceneWalk()
        {
            s_SceneWalks.fetch_add(1, std::memory_order_relaxed);
        }

        void CountGameObjectRead()
        {
            s_GameObjectReads.fetch_add(1, std::memory_order_relaxed);
        }

        void CountColliderRead()
        {
            s_ColliderReads.fetch_add(1, std::memory_order_relaxed);
        }

        Matrix4 BuildColliderWorldTransform(const SceneComponent& rootComponent)
        {
            // Collider debug draw uses world pose; scale comes from collider extents, not GO scale.
            const Transform worldTransform = rootComponent.GetWorldTransform();
            Matrix4 worldMatrix = glm::translate(glm::mat4(1.0f), worldTransform.Position);
            worldMatrix *= glm::mat4_cast(worldTransform.Rotation.ToGlm());
            return worldMatrix;
        }

        Vector4 GetColorForChannel(const ECollisionChannel channel)
        {
            switch (channel)
            {
            case ECollisionChannel::WorldStatic:
                return Vector4(0.4f, 0.8f, 0.4f, 1.0f);
            case ECollisionChannel::Default:
                return Vector4(0.2f, 0.9f, 1.0f, 1.0f);
            case ECollisionChannel::Trigger:
                return Vector4(1.0f, 0.9f, 0.2f, 1.0f);
            default:
                return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            }
        }

        void AppendCollider(
            const ColliderComponent& colliderComponent,
            const SceneComponent& rootComponent,
            DebugDrawSnapshot& outSnapshot)
        {
            CountColliderRead();

            const Matrix4 worldTransform = BuildColliderWorldTransform(rootComponent);
            const Vector4 color = GetColorForChannel(colliderComponent.GetObjectChannel());

            if (colliderComponent.IsA(BoxColliderComponent::StaticClass()))
            {
                const auto& boxCollider = static_cast<const BoxColliderComponent&>(colliderComponent);
                DebugBoxCommand command;
                command.WorldTransform = worldTransform;
                command.HalfExtent = boxCollider.GetHalfExtent();
                command.Color = color;
                outSnapshot.Boxes.push_back(command);
                return;
            }

            if (colliderComponent.IsA(SphereColliderComponent::StaticClass()))
            {
                const auto& sphereCollider = static_cast<const SphereColliderComponent&>(colliderComponent);
                DebugSphereCommand command;
                command.WorldTransform = worldTransform;
                command.Radius = sphereCollider.GetRadius();
                command.Color = color;
                outSnapshot.Spheres.push_back(command);
                return;
            }

            if (colliderComponent.IsA(CapsuleColliderComponent::StaticClass()))
            {
                const auto& capsuleCollider = static_cast<const CapsuleColliderComponent&>(colliderComponent);
                DebugCapsuleCommand command;
                command.WorldTransform = worldTransform;
                command.Radius = capsuleCollider.GetRadius();
                command.HalfHeight = capsuleCollider.GetHalfHeight();
                command.Color = color;
                outSnapshot.Capsules.push_back(command);
            }
        }
    }

    AccessProbe ProbeGameObjectAccess()
    {
        AccessProbe probe;
        probe.SceneWalks = s_SceneWalks.load(std::memory_order_relaxed);
        probe.GameObjectReads = s_GameObjectReads.load(std::memory_order_relaxed);
        probe.ColliderReads = s_ColliderReads.load(std::memory_order_relaxed);
        return probe;
    }

    DebugDrawSnapshotRef BuildSnapshot(const Scene& scene, const Options& options)
    {
        if (!options.bDrawColliders)
        {
            return nullptr;
        }

        DebugDrawSnapshot snapshot;
        CountSceneWalk();
        for (const std::shared_ptr<GameObject>& gameObject : scene.GetAllGameObjects())
        {
            if (!gameObject)
            {
                continue;
            }

            CountGameObjectRead();

            SceneComponent* rootComponent = gameObject->GetRootComponent();
            if (rootComponent == nullptr)
            {
                continue;
            }

            for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
            {
                if (!component || !component->GetClass() || !component->IsA(ColliderComponent::StaticClass()))
                {
                    continue;
                }

                AppendCollider(static_cast<const ColliderComponent&>(*component), *rootComponent, snapshot);
            }
        }

        if (snapshot.IsEmpty())
        {
            return nullptr;
        }

        return std::make_shared<const DebugDrawSnapshot>(std::move(snapshot));
    }
}
