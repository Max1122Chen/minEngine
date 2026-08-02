#include "PhysicsWorld.h"

#include "PhysicsConversion.h"
#include "RigidBodyComponent.h"
#include "BoxColliderComponent.h"
#include "SphereColliderComponent.h"
#include "CapsuleColliderComponent.h"
#include "ColliderComponent.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>
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

        JPH::ObjectLayer ToJoltObjectLayer(ECollisionChannel channel)
        {
            return static_cast<JPH::ObjectLayer>(channel);
        }

        uint64_t MakeContactKey(const JPH::BodyID& bodyA, const JPH::BodyID& bodyB)
        {
            const uint32_t indexA = bodyA.GetIndex();
            const uint32_t indexB = bodyB.GetIndex();
            const uint32_t low = indexA < indexB ? indexA : indexB;
            const uint32_t high = indexA < indexB ? indexB : indexA;
            return (static_cast<uint64_t>(low) << 32) | static_cast<uint64_t>(high);
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
        static constexpr JPH::BroadPhaseLayer NonMovingBroadPhaseLayer{0};
        static constexpr JPH::BroadPhaseLayer MovingBroadPhaseLayer{1};
        static constexpr JPH::uint BroadPhaseLayerCount = 2;
        static constexpr JPH::ObjectLayer ChannelCount =
            static_cast<JPH::ObjectLayer>(ECollisionChannel::MAX);

        struct ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
        {
            bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
            {
                if (inObject1 >= ChannelCount || inObject2 >= ChannelCount)
                {
                    return false;
                }

                const ECollisionResponse response = CollisionChannelRegistry::Get().GetResponse(
                    static_cast<ECollisionChannel>(inObject1),
                    static_cast<ECollisionChannel>(inObject2));
                return response != ECollisionResponse::Ignore;
            }
        };

        struct BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
        {
            JPH::uint GetNumBroadPhaseLayers() const override
            {
                return Impl::BroadPhaseLayerCount;
            }

            JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
            {
                if (inLayer == static_cast<JPH::ObjectLayer>(ECollisionChannel::WorldStatic))
                {
                    return Impl::NonMovingBroadPhaseLayer;
                }

                return Impl::MovingBroadPhaseLayer;
            }
        };

        struct ObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
        {
            bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
            {
                if (inLayer1 == static_cast<JPH::ObjectLayer>(ECollisionChannel::WorldStatic))
                {
                    return inLayer2 == Impl::MovingBroadPhaseLayer;
                }

                return true;
            }
        };

        struct ContactListenerImpl final : public JPH::ContactListener
        {
            Impl* Owner = nullptr;

            void OnContactAdded(
                const JPH::Body& inBody1,
                const JPH::Body& inBody2,
                const JPH::ContactManifold& inManifold,
                JPH::ContactSettings& ioSettings) override
            {
                (void)inManifold;
                (void)ioSettings;
                if (Owner == nullptr)
                {
                    return;
                }

                const ECollisionChannel channelA =
                    static_cast<ECollisionChannel>(inBody1.GetObjectLayer());
                const ECollisionChannel channelB =
                    static_cast<ECollisionChannel>(inBody2.GetObjectLayer());
                const ECollisionResponse response =
                    CollisionChannelRegistry::Get().GetResponse(channelA, channelB);

                const JPH::BodyID bodyIdA = inBody1.GetID();
                const JPH::BodyID bodyIdB = inBody2.GetID();
                const uint64_t contactKey = MakeContactKey(bodyIdA, bodyIdB);
                Owner->m_ActiveContactResponses[contactKey] = response;

                PhysicsContactEvent contactEvent;
                contactEvent.BodyA = bodyIdA.GetIndex();
                contactEvent.BodyB = bodyIdB.GetIndex();
                contactEvent.Response = response;
                contactEvent.Phase = EContactPhase::Begin;
                Owner->m_WriteContacts.push_back(contactEvent);
            }

            void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override
            {
                if (Owner == nullptr)
                {
                    return;
                }

                const JPH::BodyID bodyIdA = inSubShapePair.GetBody1ID();
                const JPH::BodyID bodyIdB = inSubShapePair.GetBody2ID();
                const uint64_t contactKey = MakeContactKey(bodyIdA, bodyIdB);

                ECollisionResponse response = ECollisionResponse::Block;
                const auto activeIt = Owner->m_ActiveContactResponses.find(contactKey);
                if (activeIt != Owner->m_ActiveContactResponses.end())
                {
                    response = activeIt->second;
                    Owner->m_ActiveContactResponses.erase(activeIt);
                }

                PhysicsContactEvent contactEvent;
                contactEvent.BodyA = bodyIdA.GetIndex();
                contactEvent.BodyB = bodyIdB.GetIndex();
                contactEvent.Response = response;
                contactEvent.Phase = EContactPhase::End;
                Owner->m_WriteContacts.push_back(contactEvent);
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
        ContactListenerImpl m_ContactListener;
        // Job/temp allocators must outlive m_PhysicsSystem (destroyed first in ~Impl).
        // Single-threaded job system avoids worker-thread issues with OpenGL on the main thread in Editor.
        JPH::JobSystemSingleThreaded m_JobSystem{JPH::cMaxPhysicsJobs};
        JPH::TempAllocatorImpl m_TempAllocator{10 * 1024 * 1024};
        JPH::PhysicsSystem m_PhysicsSystem;

        float m_Accumulator = 0.0f;
        bool m_BroadPhaseOptimized = false;
        std::vector<RigidBodyEntry> m_RigidBodies;
        std::vector<PhysicsContactEvent> m_WriteContacts;
        std::vector<PhysicsContactEvent> m_ReadContacts;
        std::unordered_map<uint64_t, ECollisionResponse> m_ActiveContactResponses;

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

            m_ContactListener.Owner = this;
            m_PhysicsSystem.SetContactListener(&m_ContactListener);

            const Vector3 engineGravity(0.0f, -9.81f, 0.0f);
            const Vector3 joltGravity = PhysicsConversion::ToJoltPosition(engineGravity);
            m_PhysicsSystem.SetGravity(JPH::Vec3(joltGravity.x, joltGravity.y, joltGravity.z));
        }

        ~Impl()
        {
            m_PhysicsSystem.SetContactListener(nullptr);
            m_ContactListener.Owner = nullptr;
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

        RigidBodyEntry* FindEntryByBodyId(const JPH::BodyID& bodyId)
        {
            for (RigidBodyEntry& entry : m_RigidBodies)
            {
                if (entry.BodyId == bodyId)
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

        void SwapContactBuffers()
        {
            m_ReadContacts.swap(m_WriteContacts);
            m_WriteContacts.clear();
        }

        bool CastShapeTrace(
            const Vector3& start,
            const Vector3& end,
            const JPH::Shape* shape,
            ECollisionChannel traceChannel,
            const CollisionQueryParams& params,
            HitResult& outHit);
    };

    bool PhysicsWorld::Impl::CastShapeTrace(
        const Vector3& start,
        const Vector3& end,
        const JPH::Shape* shape,
        ECollisionChannel traceChannel,
        const CollisionQueryParams& params,
        HitResult& outHit)
    {
        outHit = HitResult{};
        if (shape == nullptr || traceChannel >= ECollisionChannel::MAX)
        {
            return false;
        }

        const Vector3 joltStart = PhysicsConversion::ToJoltPosition(start);
        const Vector3 joltEnd = PhysicsConversion::ToJoltPosition(end);
        const Vector3 joltDelta = joltEnd - joltStart;
        const float joltLengthSq =
            joltDelta.x * joltDelta.x + joltDelta.y * joltDelta.y + joltDelta.z * joltDelta.z;
        if (joltLengthSq <= 0.0f)
        {
            return false;
        }

        struct TraceObjectLayerFilter final : public JPH::ObjectLayerFilter
        {
            ECollisionChannel TraceChannel = ECollisionChannel::Default;

            bool ShouldCollide(JPH::ObjectLayer inLayer) const override
            {
                if (inLayer >= static_cast<JPH::ObjectLayer>(ECollisionChannel::MAX))
                {
                    return false;
                }

                const ECollisionResponse response = CollisionChannelRegistry::Get().GetResponse(
                    TraceChannel,
                    static_cast<ECollisionChannel>(inLayer));
                return response != ECollisionResponse::Ignore;
            }
        };

        struct IgnoreGameObjectBodyFilter final : public JPH::BodyFilter
        {
            Impl* Owner = nullptr;
            GameObject* IgnoreGameObject = nullptr;

            bool ShouldCollide(const JPH::BodyID& inBodyID) const override
            {
                if (IgnoreGameObject == nullptr || Owner == nullptr)
                {
                    return true;
                }

                const Impl::RigidBodyEntry* entry = Owner->FindEntryByBodyId(inBodyID);
                if (entry == nullptr || entry->Component == nullptr)
                {
                    return true;
                }

                return entry->Component->GetOwner() != IgnoreGameObject;
            }
        };

        TraceObjectLayerFilter objectLayerFilter;
        objectLayerFilter.TraceChannel = traceChannel;

        IgnoreGameObjectBodyFilter bodyFilter;
        bodyFilter.Owner = this;
        bodyFilter.IgnoreGameObject = params.IgnoreGameObject;

        const JPH::RMat44 startTransform = JPH::RMat44::sTranslation(
            JPH::RVec3(joltStart.x, joltStart.y, joltStart.z));
        const JPH::RShapeCast shapeCast(
            shape,
            JPH::Vec3::sOne(),
            startTransform,
            JPH::Vec3(joltDelta.x, joltDelta.y, joltDelta.z));

        JPH::ShapeCastSettings castSettings;
        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
        m_PhysicsSystem.GetNarrowPhaseQuery().CastShape(
            shapeCast,
            castSettings,
            JPH::RVec3::sZero(),
            collector,
            {},
            objectLayerFilter,
            bodyFilter);

        if (!collector.HadHit())
        {
            return false;
        }

        const JPH::ShapeCastResult& castHit = collector.mHit;
        const Vector3 engineHitPoint = PhysicsConversion::FromJoltPosition(
            Vector3(
                castHit.mContactPointOn2.GetX(),
                castHit.mContactPointOn2.GetY(),
                castHit.mContactPointOn2.GetZ()));

        Vector3 engineNormal(0.0f, 1.0f, 0.0f);
        const float axisLengthSq = castHit.mPenetrationAxis.LengthSq();
        if (axisLengthSq > 0.0f)
        {
            const JPH::Vec3 joltNormal = -castHit.mPenetrationAxis.Normalized();
            engineNormal = PhysicsConversion::FromJoltPosition(
                Vector3(joltNormal.GetX(), joltNormal.GetY(), joltNormal.GetZ()));
            const float normalLengthSq =
                engineNormal.x * engineNormal.x
                + engineNormal.y * engineNormal.y
                + engineNormal.z * engineNormal.z;
            if (normalLengthSq > 0.0f)
            {
                engineNormal = engineNormal * (1.0f / std::sqrt(normalLengthSq));
            }
        }

        ECollisionChannel objectChannel = ECollisionChannel::Default;
        {
            JPH::BodyLockRead bodyLock(m_PhysicsSystem.GetBodyLockInterface(), castHit.mBodyID2);
            if (bodyLock.Succeeded())
            {
                objectChannel = static_cast<ECollisionChannel>(bodyLock.GetBody().GetObjectLayer());
            }
        }

        const ECollisionResponse response =
            CollisionChannelRegistry::Get().GetResponse(traceChannel, objectChannel);
        const Vector3 engineDelta = end - start;
        const float engineDistance = std::sqrt(
            engineDelta.x * engineDelta.x
            + engineDelta.y * engineDelta.y
            + engineDelta.z * engineDelta.z);

        outHit.bHit = true;
        outHit.bBlockingHit = response == ECollisionResponse::Block;
        outHit.Location = engineHitPoint;
        outHit.Normal = engineNormal;
        outHit.Time = castHit.mFraction;
        outHit.Distance = engineDistance * castHit.mFraction;
        outHit.BodyId = castHit.mBodyID2.GetIndex();

        if (const RigidBodyEntry* entry = FindEntryByBodyId(castHit.mBodyID2))
        {
            outHit.RigidBody = entry->Component;
            if (entry->Component != nullptr)
            {
                outHit.HitObject = entry->Component->GetOwner();
                outHit.Collider = entry->Component->FindColliderComponent();
            }
        }

        return true;
    }

    PhysicsWorld::PhysicsWorld()
        : m_Impl(std::make_unique<Impl>())
    {
    }

    PhysicsWorld::~PhysicsWorld()
    {
        UnregisterAllRigidBodies();
    }

    const std::vector<PhysicsContactEvent>& PhysicsWorld::GetContactEvents() const
    {
        return m_Impl->m_ReadContacts;
    }

    bool PhysicsWorld::LineTrace(
        const Vector3& start,
        const Vector3& end,
        ECollisionChannel traceChannel,
        const CollisionQueryParams& params,
        HitResult& outHit)
    {
        outHit = HitResult{};

        if (traceChannel >= ECollisionChannel::MAX)
        {
            return false;
        }

        const Vector3 joltStart = PhysicsConversion::ToJoltPosition(start);
        const Vector3 joltEnd = PhysicsConversion::ToJoltPosition(end);
        const Vector3 joltDelta = joltEnd - joltStart;
        const float joltLengthSq =
            joltDelta.x * joltDelta.x + joltDelta.y * joltDelta.y + joltDelta.z * joltDelta.z;
        if (joltLengthSq <= 0.0f)
        {
            return false;
        }

        struct TraceObjectLayerFilter final : public JPH::ObjectLayerFilter
        {
            ECollisionChannel TraceChannel = ECollisionChannel::Default;

            bool ShouldCollide(JPH::ObjectLayer inLayer) const override
            {
                if (inLayer >= static_cast<JPH::ObjectLayer>(ECollisionChannel::MAX))
                {
                    return false;
                }

                const ECollisionResponse response = CollisionChannelRegistry::Get().GetResponse(
                    TraceChannel,
                    static_cast<ECollisionChannel>(inLayer));
                return response != ECollisionResponse::Ignore;
            }
        };

        struct IgnoreGameObjectBodyFilter final : public JPH::BodyFilter
        {
            Impl* Owner = nullptr;
            GameObject* IgnoreGameObject = nullptr;

            bool ShouldCollide(const JPH::BodyID& inBodyID) const override
            {
                if (IgnoreGameObject == nullptr || Owner == nullptr)
                {
                    return true;
                }

                const Impl::RigidBodyEntry* entry = Owner->FindEntryByBodyId(inBodyID);
                if (entry == nullptr || entry->Component == nullptr)
                {
                    return true;
                }

                return entry->Component->GetOwner() != IgnoreGameObject;
            }
        };

        TraceObjectLayerFilter objectLayerFilter;
        objectLayerFilter.TraceChannel = traceChannel;

        IgnoreGameObjectBodyFilter bodyFilter;
        bodyFilter.Owner = m_Impl.get();
        bodyFilter.IgnoreGameObject = params.IgnoreGameObject;

        const JPH::RRayCast ray(
            JPH::RVec3(joltStart.x, joltStart.y, joltStart.z),
            JPH::Vec3(joltDelta.x, joltDelta.y, joltDelta.z));

        JPH::RayCastResult castResult;
        const bool hasHit = m_Impl->m_PhysicsSystem.GetNarrowPhaseQuery().CastRay(
            ray,
            castResult,
            {},
            objectLayerFilter,
            bodyFilter);
        if (!hasHit)
        {
            return false;
        }

        const JPH::RVec3 joltHitPoint = ray.GetPointOnRay(castResult.mFraction);
        const Vector3 engineHitPoint = PhysicsConversion::FromJoltPosition(
            Vector3(
                static_cast<float>(joltHitPoint.GetX()),
                static_cast<float>(joltHitPoint.GetY()),
                static_cast<float>(joltHitPoint.GetZ())));

        Vector3 engineNormal(0.0f, 1.0f, 0.0f);
        ECollisionChannel objectChannel = ECollisionChannel::Default;
        {
            JPH::BodyLockRead bodyLock(
                m_Impl->m_PhysicsSystem.GetBodyLockInterface(),
                castResult.mBodyID);
            if (bodyLock.Succeeded())
            {
                const JPH::Body& body = bodyLock.GetBody();
                objectChannel = static_cast<ECollisionChannel>(body.GetObjectLayer());
                const JPH::Vec3 joltNormal = body.GetWorldSpaceSurfaceNormal(
                    castResult.mSubShapeID2,
                    JPH::RVec3(joltHitPoint));
                engineNormal = PhysicsConversion::FromJoltPosition(
                    Vector3(joltNormal.GetX(), joltNormal.GetY(), joltNormal.GetZ()));
                const float normalLengthSq =
                    engineNormal.x * engineNormal.x
                    + engineNormal.y * engineNormal.y
                    + engineNormal.z * engineNormal.z;
                if (normalLengthSq > 0.0f)
                {
                    const float invLength = 1.0f / std::sqrt(normalLengthSq);
                    engineNormal = engineNormal * invLength;
                }
            }
        }

        const ECollisionResponse response =
            CollisionChannelRegistry::Get().GetResponse(traceChannel, objectChannel);

        const Vector3 engineDelta = end - start;
        const float engineDistance = std::sqrt(
            engineDelta.x * engineDelta.x
            + engineDelta.y * engineDelta.y
            + engineDelta.z * engineDelta.z);

        outHit.bHit = true;
        outHit.bBlockingHit = response == ECollisionResponse::Block;
        outHit.Location = engineHitPoint;
        outHit.Normal = engineNormal;
        outHit.Time = castResult.mFraction;
        outHit.Distance = engineDistance * castResult.mFraction;
        outHit.BodyId = castResult.mBodyID.GetIndex();

        if (const Impl::RigidBodyEntry* entry = m_Impl->FindEntryByBodyId(castResult.mBodyID))
        {
            outHit.RigidBody = entry->Component;
            if (entry->Component != nullptr)
            {
                outHit.HitObject = entry->Component->GetOwner();
                outHit.Collider = entry->Component->FindColliderComponent();
            }
        }

        return true;
    }

    bool PhysicsWorld::SphereTrace(
        const Vector3& start,
        const Vector3& end,
        float radius,
        ECollisionChannel traceChannel,
        const CollisionQueryParams& params,
        HitResult& outHit)
    {
        outHit = HitResult{};
        if (radius <= 0.0f || traceChannel >= ECollisionChannel::MAX)
        {
            return false;
        }

        JPH::SphereShapeSettings sphereSettings(radius);
        JPH::ShapeSettings::ShapeResult shapeResult = sphereSettings.Create();
        if (shapeResult.HasError())
        {
            return false;
        }

        return m_Impl->CastShapeTrace(
            start,
            end,
            shapeResult.Get(),
            traceChannel,
            params,
            outHit);
    }

    bool PhysicsWorld::CapsuleTrace(
        const Vector3& start,
        const Vector3& end,
        float radius,
        float halfHeight,
        ECollisionChannel traceChannel,
        const CollisionQueryParams& params,
        HitResult& outHit)
    {
        outHit = HitResult{};
        if (radius <= 0.0f || halfHeight < 0.0f || traceChannel >= ECollisionChannel::MAX)
        {
            return false;
        }

        JPH::CapsuleShapeSettings capsuleSettings(halfHeight, radius);
        JPH::ShapeSettings::ShapeResult shapeResult = capsuleSettings.Create();
        if (shapeResult.HasError())
        {
            return false;
        }

        return m_Impl->CastShapeTrace(
            start,
            end,
            shapeResult.Get(),
            traceChannel,
            params,
            outHit);
    }

    void PhysicsWorld::RegisterRigidBody(
        RigidBodyComponent* rigidBodyComponent,
        ColliderComponent* colliderComponent)
    {
        if (rigidBodyComponent == nullptr || colliderComponent == nullptr
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
        JPH::ShapeSettings::ShapeResult shapeResult;
        if (colliderComponent->IsA(BoxColliderComponent::StaticClass()))
        {
            const auto* boxCollider = static_cast<const BoxColliderComponent*>(colliderComponent);
            const Vector3 engineHalfExtent = boxCollider->GetHalfExtent() * uniformScale;
            const Vector3 joltHalfExtent = PhysicsConversion::ToJoltPosition(engineHalfExtent);
            JPH::BoxShapeSettings boxSettings(
                JPH::Vec3(joltHalfExtent.x, joltHalfExtent.y, joltHalfExtent.z));
            shapeResult = boxSettings.Create();
        }
        else if (colliderComponent->IsA(SphereColliderComponent::StaticClass()))
        {
            const auto* sphereCollider = static_cast<const SphereColliderComponent*>(colliderComponent);
            JPH::SphereShapeSettings sphereSettings(sphereCollider->GetRadius() * uniformScale);
            shapeResult = sphereSettings.Create();
        }
        else if (colliderComponent->IsA(CapsuleColliderComponent::StaticClass()))
        {
            const auto* capsuleCollider = static_cast<const CapsuleColliderComponent*>(colliderComponent);
            JPH::CapsuleShapeSettings capsuleShapeSettings(
                capsuleCollider->GetHalfHeight() * uniformScale,
                capsuleCollider->GetRadius() * uniformScale);
            shapeResult = capsuleShapeSettings.Create();
        }
        else
        {
            ME_CORE_ERROR("PhysicsWorld: unsupported collider type.");
            return;
        }

        if (shapeResult.HasError())
        {
            ME_CORE_ERROR("PhysicsWorld: failed to create collider shape.");
            return;
        }

        const Vector3 enginePosition = rootComponent->GetPosition();
        const Vector3 joltPosition = PhysicsConversion::ToJoltPosition(enginePosition);
        const Quaternion joltRotation = PhysicsConversion::ToJoltQuaternion(rootComponent->GetRotation());
        const ECollisionChannel objectChannel = colliderComponent->GetObjectChannel();

        JPH::BodyCreationSettings bodySettings(
            shapeResult.Get(),
            JPH::RVec3(joltPosition.x, joltPosition.y, joltPosition.z),
            JPH::Quat(joltRotation.X, joltRotation.Y, joltRotation.Z, joltRotation.W),
            ToJoltMotionType(rigidBodyComponent->GetBodyType()),
            ToJoltObjectLayer(objectChannel));

        bodySettings.mIsSensor = objectChannel == ECollisionChannel::Trigger;

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

        m_Impl->m_WriteContacts.clear();
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

        m_Impl->SwapContactBuffers();
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
