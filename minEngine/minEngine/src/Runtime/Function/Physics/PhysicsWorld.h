#pragma once

#include "Core.h"
#include "Runtime/Function/Physics/PhysicsTypes.h"

#include <memory>
#include <vector>

namespace minEngine
{
    class RigidBodyComponent;
    class ColliderComponent;
    class GameObject;

    class PhysicsWorld
    {
    public:
        PhysicsWorld();
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;

        void Step(float deltaTime);
        void SyncBodiesFromScene();
        void SyncBodiesToScene();

        const std::vector<PhysicsContactEvent>& GetContactEvents() const;

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

        void RegisterRigidBody(RigidBodyComponent* rigidBodyComponent, ColliderComponent* colliderComponent);
        void UnregisterRigidBody(RigidBodyComponent* rigidBodyComponent);
        void OnRigidBodySimulatePhysicsChanged(RigidBodyComponent* rigidBodyComponent);

    private:
        void UnregisterAllRigidBodies();

        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
