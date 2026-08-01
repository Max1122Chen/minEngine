#pragma once

#include "Core.h"
#include "Runtime/Function/Physics/PhysicsTypes.h"

#include <memory>
#include <vector>

namespace minEngine
{
    class RigidBodyComponent;
    class BoxColliderComponent;

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

        /** Contact/overlap events from the last completed Step (read buffer). */
        const std::vector<FPhysicsContactEvent>& GetContactEvents() const;

        void RegisterRigidBody(RigidBodyComponent* rigidBodyComponent, BoxColliderComponent* boxColliderComponent);
        void UnregisterRigidBody(RigidBodyComponent* rigidBodyComponent);
        void OnRigidBodySimulatePhysicsChanged(RigidBodyComponent* rigidBodyComponent);

    private:
        void UnregisterAllRigidBodies();

        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
