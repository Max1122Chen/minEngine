#include "PhysicsWorld.h"

#include "PhysicsConversion.h"
#include "RigidBodyComponent.h"
#include "BoxColliderComponent.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>

#include <algorithm>
#include <vector>

JPH_SUPPRESS_WARNINGS

namespace minEngine
{
    namespace
    {
        float GetUniformScale(const Vector3& scale)
        {
            return scale.x;
        }

        JPH::EMotionType ToJoltMotionType(EBodyType bodyType)
        {
            switch (bodyType)
            {
            case EBodyType::Static:
                return JPH::EMotionType::Static;
            case EBodyType::Kinematic:
                return JPH::EMotionType::Kinematic;
            case EBodyType::Dynamic:
            default:
                return JPH::EMotionType::Dynamic;
            }
        }

        JPH::ObjectLayer ToJoltObjectLayer(EBodyType bodyType)
        {
            return bodyType == EBodyType::Static ? 0u : 1u;
        }

        void PushBodyPoseFromScene(
            JPH::BodyInterface& bodyInterface,
            const JPH::BodyID& bodyId,
            const SceneComponent& rootComponent,
            ETeleportType teleportType)
        {
            const Vector3 enginePosition = rootComponent.GetPosition();
            const Vector3 joltPosition = PhysicsConversion::ToJoltPosition(enginePosition);
            const Quaternion joltRotation = PhysicsConversion::ToJoltQuaternion(rootComponent.GetRotation());

            bodyInterface.SetPositionAndRotation(
                bodyId,
                JPH::RVec3(joltPosition.x, joltPosition.y, joltPosition.z),
                JPH::Quat(joltRotation.X, joltRotation.Y, joltRotation.Z, joltRotation.W),
                JPH::EActivation::Activate);

            if (teleportType == ETeleportType::ResetPhysics)
            {
                bodyInterface.SetLinearVelocity(bodyId, JPH::Vec3::sZero());
                bodyInterface.SetAngularVelocity(bodyId, JPH::Vec3::sZero());
            }
        }
    }

    struct PhysicsWorld::Impl
    {
        static constexpr JPH::ObjectLayer NonMovingObjectLayer = 0;
        static constexpr JPH::ObjectLayer MovingObjectLayer = 1;
        static constexpr JPH::BroadPhaseLayer NonMovingBroadPhaseLayer{0};
        static constexpr JPH::BroadPhaseLayer MovingBroadPhaseLayer{1};
        static constexpr JPH::uint BroadPhaseLayerCount = 2;

        struct ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
        {
            bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
            {
                switch (inObject1)
                {
                case Impl::NonMovingObjectLayer:
                    return inObject2 == Impl::MovingObjectLayer;
                case Impl::MovingObjectLayer:
                    return true;
                default:
                    return false;
                }
            }
        };

        struct BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
        {
            BroadPhaseLayerInterface()
            {
                m_ObjectToBroadPhase[Impl::NonMovingObjectLayer] = Impl::NonMovingBroadPhaseLayer;
                m_ObjectToBroadPhase[Impl::MovingObjectLayer] = Impl::MovingBroadPhaseLayer;
            }

            JPH::uint GetNumBroadPhaseLayers() const override
            {
                return Impl::BroadPhaseLayerCount;
            }

            JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
            {
                return m_ObjectToBroadPhase[inLayer];
            }

            JPH::BroadPhaseLayer m_ObjectToBroadPhase[2]{};
        };

        struct ObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
        {
            bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
            {
                switch (inLayer1)
                {
                case Impl::NonMovingObjectLayer:
                    return inLayer2 == Impl::MovingBroadPhaseLayer;
                case Impl::MovingObjectLayer:
                    return true;
                default:
                    return false;
                }
            }
        };

        struct RigidBodyEntry
        {
            RigidBodyComponent* Component = nullptr;
            JPH::BodyID BodyId;
        };

        static constexpr float FixedTimeStep = 1.0f / 60.0f;
        static constexpr int MaxSubSteps = 4;
        static constexpr JPH::uint MaxBodies = 1024;
        static constexpr JPH::uint MaxBodyPairs = 1024;
        static constexpr JPH::uint MaxContactConstraints = 1024;

        BroadPhaseLayerInterface m_BroadPhaseLayerInterface;
        ObjectVsBroadPhaseLayerFilter m_ObjectVsBroadPhaseLayerFilter;
        ObjectLayerPairFilter m_ObjectLayerPairFilter;
        // Job/temp allocators must outlive m_PhysicsSystem (destroyed first in ~Impl).
        // Single-threaded job system avoids worker-thread issues with OpenGL on the main thread in Editor.
        JPH::JobSystemSingleThreaded m_JobSystem{JPH::cMaxPhysicsJobs};
        JPH::TempAllocatorImpl m_TempAllocator{10 * 1024 * 1024};
        JPH::PhysicsSystem m_PhysicsSystem;

        float m_Accumulator = 0.0f;
        bool m_BroadPhaseOptimized = false;
        std::vector<RigidBodyEntry> m_RigidBodies;

        Impl()
        {
            m_PhysicsSystem.Init(
                MaxBodies,
                0,
                MaxBodyPairs,
                MaxContactConstraints,
                m_BroadPhaseLayerInterface,
                m_ObjectVsBroadPhaseLayerFilter,
                m_ObjectLayerPairFilter);

            const Vector3 engineGravity(0.0f, -9.81f, 0.0f);
            const Vector3 joltGravity = PhysicsConversion::ToJoltPosition(engineGravity);
            m_PhysicsSystem.SetGravity(JPH::Vec3(joltGravity.x, joltGravity.y, joltGravity.z));
        }

        RigidBodyEntry* FindEntry(RigidBodyComponent* component)
        {
            for (RigidBodyEntry& entry : m_RigidBodies)
            {
                if (entry.Component == component)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        void DestroyJoltBody(const JPH::BodyID& bodyId)
        {
            if (bodyId.IsInvalid())
            {
                return;
            }

            JPH::BodyInterface& bodyInterface = m_PhysicsSystem.GetBodyInterface();
            if (bodyInterface.IsAdded(bodyId))
            {
                bodyInterface.RemoveBody(bodyId);
            }
            bodyInterface.DestroyBody(bodyId);
        }
    };

    PhysicsWorld::PhysicsWorld()
        : m_Impl(std::make_unique<Impl>())
    {
    }

    PhysicsWorld::~PhysicsWorld()
    {
        UnregisterAllRigidBodies();
    }

    void PhysicsWorld::RegisterRigidBody(
        RigidBodyComponent* rigidBodyComponent,
        BoxColliderComponent* boxColliderComponent)
    {
        if (rigidBodyComponent == nullptr || boxColliderComponent == nullptr
            || m_Impl->FindEntry(rigidBodyComponent) != nullptr)
        {
            return;
        }

        SceneComponent* rootComponent = rigidBodyComponent->GetTargetSceneComponent();
        if (rootComponent == nullptr)
        {
            return;
        }

        const float uniformScale = GetUniformScale(rootComponent->GetScale());
        const Vector3 engineHalfExtent = boxColliderComponent->GetHalfExtent() * uniformScale;
        const Vector3 joltHalfExtent = PhysicsConversion::ToJoltPosition(engineHalfExtent);

        JPH::BoxShapeSettings boxSettings(
            JPH::Vec3(joltHalfExtent.x, joltHalfExtent.y, joltHalfExtent.z));
        JPH::ShapeSettings::ShapeResult shapeResult = boxSettings.Create();
        if (shapeResult.HasError())
        {
            ME_CORE_ERROR("PhysicsWorld: failed to create box shape.");
            return;
        }

        const Vector3 enginePosition = rootComponent->GetPosition();
        const Vector3 joltPosition = PhysicsConversion::ToJoltPosition(enginePosition);
        const Quaternion joltRotation = PhysicsConversion::ToJoltQuaternion(rootComponent->GetRotation());

        JPH::BodyCreationSettings bodySettings(
            shapeResult.Get(),
            JPH::RVec3(joltPosition.x, joltPosition.y, joltPosition.z),
            JPH::Quat(joltRotation.X, joltRotation.Y, joltRotation.Z, joltRotation.W),
            ToJoltMotionType(rigidBodyComponent->GetBodyType()),
            ToJoltObjectLayer(rigidBodyComponent->GetBodyType()));

        if (rigidBodyComponent->GetBodyType() == EBodyType::Dynamic)
        {
            bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            bodySettings.mMassPropertiesOverride.mMass = rigidBodyComponent->GetMass();
        }

        JPH::BodyInterface& bodyInterface = m_Impl->m_PhysicsSystem.GetBodyInterface();
        JPH::Body* body = bodyInterface.CreateBody(bodySettings);
        if (body == nullptr)
        {
            ME_CORE_ERROR("PhysicsWorld: failed to create rigid body.");
            return;
        }

        const JPH::BodyID bodyId = body->GetID();
        const JPH::EActivation activation =
            rigidBodyComponent->GetBodyType() == EBodyType::Dynamic ? JPH::EActivation::Activate
                                                                    : JPH::EActivation::DontActivate;
        bodyInterface.AddBody(bodyId, activation);

        if (rigidBodyComponent->GetBodyType() == EBodyType::Dynamic
            && !rigidBodyComponent->GetSimulatePhysics())
        {
            bodyInterface.DeactivateBody(bodyId);
        }

        Impl::RigidBodyEntry entry;
        entry.Component = rigidBodyComponent;
        entry.BodyId = bodyId;
        m_Impl->m_RigidBodies.push_back(entry);
        rigidBodyComponent->SetPhysicsBodyId(bodyId.GetIndex());

        if (!m_Impl->m_BroadPhaseOptimized)
        {
            m_Impl->m_PhysicsSystem.OptimizeBroadPhase();
            m_Impl->m_BroadPhaseOptimized = true;
        }
    }

    void PhysicsWorld::UnregisterRigidBody(RigidBodyComponent* rigidBodyComponent)
    {
        if (rigidBodyComponent == nullptr)
        {
            return;
        }

        Impl::RigidBodyEntry* entry = m_Impl->FindEntry(rigidBodyComponent);
        if (entry == nullptr)
        {
            rigidBodyComponent->SetPhysicsBodyId(InvalidPhysicsBodyId);
            return;
        }

        m_Impl->DestroyJoltBody(entry->BodyId);
        m_Impl->m_RigidBodies.erase(
            std::remove_if(
                m_Impl->m_RigidBodies.begin(),
                m_Impl->m_RigidBodies.end(),
                [rigidBodyComponent](const Impl::RigidBodyEntry& candidate)
                {
                    return candidate.Component == rigidBodyComponent;
                }),
            m_Impl->m_RigidBodies.end());
        rigidBodyComponent->SetPhysicsBodyId(InvalidPhysicsBodyId);
    }

    void PhysicsWorld::UnregisterAllRigidBodies()
    {
        for (const Impl::RigidBodyEntry& entry : m_Impl->m_RigidBodies)
        {
            if (entry.Component != nullptr)
            {
                m_Impl->DestroyJoltBody(entry.BodyId);
                entry.Component->SetPhysicsBodyId(InvalidPhysicsBodyId);
            }
        }
        m_Impl->m_RigidBodies.clear();
    }

    void PhysicsWorld::OnRigidBodySimulatePhysicsChanged(RigidBodyComponent* rigidBodyComponent)
    {
        if (rigidBodyComponent == nullptr)
        {
            return;
        }

        Impl::RigidBodyEntry* entry = m_Impl->FindEntry(rigidBodyComponent);
        SceneComponent* rootComponent = rigidBodyComponent->GetTargetSceneComponent();
        if (entry == nullptr || rootComponent == nullptr || entry->BodyId.IsInvalid())
        {
            return;
        }

        JPH::BodyInterface& bodyInterface = m_Impl->m_PhysicsSystem.GetBodyInterface();
        PushBodyPoseFromScene(
            bodyInterface,
            entry->BodyId,
            *rootComponent,
            ETeleportType::ResetPhysics);
        rootComponent->ClearTransformDirty();

        if (rigidBodyComponent->GetSimulatePhysics())
        {
            bodyInterface.ActivateBody(entry->BodyId);
        }
        else
        {
            bodyInterface.DeactivateBody(entry->BodyId);
        }
    }

    void PhysicsWorld::Step(float deltaTime)
    {
        if (deltaTime <= 0.0f)
        {
            return;
        }

        m_Impl->m_Accumulator += deltaTime;
        int subStepCount = 0;
        while (m_Impl->m_Accumulator >= Impl::FixedTimeStep && subStepCount < Impl::MaxSubSteps)
        {
            m_Impl->m_PhysicsSystem.Update(
                Impl::FixedTimeStep,
                1,
                &m_Impl->m_TempAllocator,
                &m_Impl->m_JobSystem);
            m_Impl->m_Accumulator -= Impl::FixedTimeStep;
            ++subStepCount;
        }
    }

    void PhysicsWorld::SyncBodiesFromScene()
    {
        JPH::BodyInterface& bodyInterface = m_Impl->m_PhysicsSystem.GetBodyInterface();

        for (const Impl::RigidBodyEntry& entry : m_Impl->m_RigidBodies)
        {
            RigidBodyComponent* rigidBodyComponent = entry.Component;
            if (rigidBodyComponent == nullptr || entry.BodyId.IsInvalid())
            {
                continue;
            }

            SceneComponent* rootComponent = rigidBodyComponent->GetTargetSceneComponent();
            if (rootComponent == nullptr)
            {
                continue;
            }

            const EBodyType bodyType = rigidBodyComponent->GetBodyType();
            const bool simulatePhysics = rigidBodyComponent->GetSimulatePhysics();

            bool shouldPush = false;
            ETeleportType teleportType = ETeleportType::ResetPhysics;

            if (bodyType == EBodyType::Static)
            {
                if (rootComponent->IsTransformDirty())
                {
                    shouldPush = true;
                    teleportType = rootComponent->GetPendingTeleportType();
                }
            }
            else if (bodyType == EBodyType::Dynamic)
            {
                if (!simulatePhysics)
                {
                    shouldPush = true;
                    teleportType = ETeleportType::ResetPhysics;
                }
                else if (rootComponent->IsTransformDirty())
                {
                    shouldPush = true;
                    teleportType = rootComponent->GetPendingTeleportType();
                }
            }

            if (shouldPush)
            {
                PushBodyPoseFromScene(bodyInterface, entry.BodyId, *rootComponent, teleportType);
                if (rootComponent->IsTransformDirty())
                {
                    rootComponent->ClearTransformDirty();
                }
            }

            if (bodyType == EBodyType::Dynamic && !simulatePhysics)
            {
                if (bodyInterface.IsActive(entry.BodyId))
                {
                    bodyInterface.DeactivateBody(entry.BodyId);
                }
            }
        }
    }

    void PhysicsWorld::SyncBodiesToScene()
    {
        JPH::BodyInterface& bodyInterface = m_Impl->m_PhysicsSystem.GetBodyInterface();

        for (const Impl::RigidBodyEntry& entry : m_Impl->m_RigidBodies)
        {
            RigidBodyComponent* rigidBodyComponent = entry.Component;
            if (rigidBodyComponent == nullptr || !rigidBodyComponent->GetSimulatePhysics())
            {
                continue;
            }

            if (rigidBodyComponent->GetBodyType() != EBodyType::Dynamic)
            {
                continue;
            }

            SceneComponent* rootComponent = rigidBodyComponent->GetTargetSceneComponent();
            if (rootComponent == nullptr || entry.BodyId.IsInvalid())
            {
                continue;
            }

            if (!bodyInterface.IsActive(entry.BodyId))
            {
                continue;
            }

            const JPH::RVec3 joltPosition = bodyInterface.GetPosition(entry.BodyId);
            const JPH::Quat joltRotation = bodyInterface.GetRotation(entry.BodyId);

            const Vector3 enginePosition = PhysicsConversion::FromJoltPosition(
                Vector3(static_cast<float>(joltPosition.GetX()),
                        static_cast<float>(joltPosition.GetY()),
                        static_cast<float>(joltPosition.GetZ())));
            const Quaternion joltRotationQuat(
                joltRotation.GetW(),
                joltRotation.GetX(),
                joltRotation.GetY(),
                joltRotation.GetZ());
            const Quaternion engineRotation = PhysicsConversion::FromJoltQuaternion(joltRotationQuat);

            Transform simulationTransform = rootComponent->GetTransform();
            simulationTransform.Position = enginePosition;
            simulationTransform.SetRotation(engineRotation);
            rootComponent->SetTransformFromSimulation(simulationTransform);
        }
    }
}
