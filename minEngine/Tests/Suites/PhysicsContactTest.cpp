#include "PhysicsContactTest.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Physics/BoxColliderComponent.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"
#include "Runtime/Function/Physics/PhysicsTypes.h"
#include "Runtime/Function/Physics/PhysicsWorld.h"
#include "Runtime/Function/Physics/RigidBodyComponent.h"

namespace minEngine
{
    class PhysicsContactTestScope
    {
    public:
        PhysicsContactTestScope()
        {
            ObjectManager::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();

            SceneManager::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            PhysicsSystem::SetInstance(&m_PhysicsSystem);
            m_PhysicsSystem.Initialize();
        }

        ~PhysicsContactTestScope()
        {
            m_SceneManager.Shutdown();
            SceneManager::SetInstance(nullptr);

            m_PhysicsSystem.Shutdown();
            PhysicsSystem::SetInstance(nullptr);

            m_ObjectManager.Shutdown();
            ObjectManager::SetInstance(nullptr);
        }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
        PhysicsSystem m_PhysicsSystem;
    };

    namespace
    {
        bool ContactMatches(
            const FPhysicsContactEvent& contactEvent,
            PhysicsBodyId bodyA,
            PhysicsBodyId bodyB,
            ECollisionResponse response,
            EContactPhase phase)
        {
            if (contactEvent.Response != response || contactEvent.Phase != phase)
            {
                return false;
            }

            return (contactEvent.BodyA == bodyA && contactEvent.BodyB == bodyB)
                || (contactEvent.BodyA == bodyB && contactEvent.BodyB == bodyA);
        }

        bool HasContactEvent(
            const std::vector<FPhysicsContactEvent>& events,
            PhysicsBodyId bodyA,
            PhysicsBodyId bodyB,
            ECollisionResponse response,
            EContactPhase phase)
        {
            for (const FPhysicsContactEvent& contactEvent : events)
            {
                if (ContactMatches(contactEvent, bodyA, bodyB, response, phase))
                {
                    return true;
                }
            }

            return false;
        }

        bool RunBlockBeginTest()
        {
            PhysicsContactTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("physics-contact-block");
            if (!scene)
            {
                ME_CORE_ERROR("PhysicsContactTest: failed to create block scene.");
                return false;
            }

            const std::shared_ptr<GameObject> floorObject = scene->CreateGameObject();
            floorObject->AddComponent<SceneComponent>();
            const std::shared_ptr<RigidBodyComponent> floorRigidBody = floorObject->AddComponent<RigidBodyComponent>();
            floorRigidBody->SetBodyType(EBodyType::Static);
            const std::shared_ptr<BoxColliderComponent> floorCollider = floorObject->AddComponent<BoxColliderComponent>();
            floorCollider->SetObjectChannel(ECollisionChannel::WorldStatic);
            floorCollider->SetHalfExtent(Vector3(5.0f, 0.5f, 5.0f));

            const std::shared_ptr<GameObject> dynamicObject = scene->CreateGameObject();
            const std::shared_ptr<SceneComponent> dynamicRoot = dynamicObject->AddComponent<SceneComponent>();
            dynamicRoot->SetPosition(Vector3(0.0f, 5.0f, 0.0f));
            const std::shared_ptr<RigidBodyComponent> dynamicRigidBody = dynamicObject->AddComponent<RigidBodyComponent>();
            dynamicRigidBody->SetBodyType(EBodyType::Dynamic);
            dynamicRigidBody->SetMass(1.0f);
            const std::shared_ptr<BoxColliderComponent> dynamicCollider = dynamicObject->AddComponent<BoxColliderComponent>();
            dynamicCollider->SetObjectChannel(ECollisionChannel::Default);

            if (!floorRigidBody->HasValidPhysicsBody() || !dynamicRigidBody->HasValidPhysicsBody())
            {
                ME_CORE_ERROR("PhysicsContactTest: block test bodies were not created.");
                return false;
            }

            PhysicsWorld& world = PhysicsSystem::Get().GetOrCreateWorld(scene.get());
            bool sawBlockBegin = false;

            constexpr float fixedDeltaTime = 1.0f / 60.0f;
            for (int stepIndex = 0; stepIndex < 120; ++stepIndex)
            {
                (void)stepIndex;
                PhysicsSystem::Get().SimulateActiveScene(fixedDeltaTime);
                if (HasContactEvent(
                        world.GetContactEvents(),
                        floorRigidBody->GetPhysicsBodyId(),
                        dynamicRigidBody->GetPhysicsBodyId(),
                        ECollisionResponse::Block,
                        EContactPhase::Begin))
                {
                    sawBlockBegin = true;
                    break;
                }
            }

            if (!sawBlockBegin)
            {
                ME_CORE_ERROR("PhysicsContactTest: expected Block Begin between Default and WorldStatic.");
                return false;
            }

            ME_CORE_INFO("PhysicsContactTest: Block Begin observed.");
            return true;
        }

        bool RunOverlapBeginEndTest()
        {
            PhysicsContactTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("physics-contact-overlap");
            if (!scene)
            {
                ME_CORE_ERROR("PhysicsContactTest: failed to create overlap scene.");
                return false;
            }

            // Tall trigger volume the dynamic box falls through (sensor → no push).
            const std::shared_ptr<GameObject> triggerObject = scene->CreateGameObject();
            const std::shared_ptr<SceneComponent> triggerRoot = triggerObject->AddComponent<SceneComponent>();
            triggerRoot->SetPosition(Vector3(0.0f, 5.0f, 0.0f));
            const std::shared_ptr<RigidBodyComponent> triggerRigidBody = triggerObject->AddComponent<RigidBodyComponent>();
            triggerRigidBody->SetBodyType(EBodyType::Static);
            const std::shared_ptr<BoxColliderComponent> triggerCollider = triggerObject->AddComponent<BoxColliderComponent>();
            triggerCollider->SetObjectChannel(ECollisionChannel::Trigger);
            triggerCollider->SetHalfExtent(Vector3(1.0f, 1.0f, 1.0f));

            const std::shared_ptr<GameObject> dynamicObject = scene->CreateGameObject();
            const std::shared_ptr<SceneComponent> dynamicRoot = dynamicObject->AddComponent<SceneComponent>();
            dynamicRoot->SetPosition(Vector3(0.0f, 10.0f, 0.0f));
            const std::shared_ptr<RigidBodyComponent> dynamicRigidBody = dynamicObject->AddComponent<RigidBodyComponent>();
            dynamicRigidBody->SetBodyType(EBodyType::Dynamic);
            dynamicRigidBody->SetMass(1.0f);
            const std::shared_ptr<BoxColliderComponent> dynamicCollider = dynamicObject->AddComponent<BoxColliderComponent>();
            dynamicCollider->SetObjectChannel(ECollisionChannel::Default);

            if (!triggerRigidBody->HasValidPhysicsBody() || !dynamicRigidBody->HasValidPhysicsBody())
            {
                ME_CORE_ERROR("PhysicsContactTest: overlap test bodies were not created.");
                return false;
            }

            PhysicsWorld& world = PhysicsSystem::Get().GetOrCreateWorld(scene.get());
            bool sawOverlapBegin = false;
            bool sawOverlapEnd = false;
            float heightAtBegin = dynamicRoot->GetPosition().y;

            constexpr float fixedDeltaTime = 1.0f / 60.0f;
            for (int stepIndex = 0; stepIndex < 240; ++stepIndex)
            {
                (void)stepIndex;
                PhysicsSystem::Get().SimulateActiveScene(fixedDeltaTime);
                const std::vector<FPhysicsContactEvent>& events = world.GetContactEvents();

                if (!sawOverlapBegin
                    && HasContactEvent(
                        events,
                        triggerRigidBody->GetPhysicsBodyId(),
                        dynamicRigidBody->GetPhysicsBodyId(),
                        ECollisionResponse::Overlap,
                        EContactPhase::Begin))
                {
                    sawOverlapBegin = true;
                    heightAtBegin = dynamicRoot->GetPosition().y;
                }

                if (sawOverlapBegin
                    && HasContactEvent(
                        events,
                        triggerRigidBody->GetPhysicsBodyId(),
                        dynamicRigidBody->GetPhysicsBodyId(),
                        ECollisionResponse::Overlap,
                        EContactPhase::End))
                {
                    sawOverlapEnd = true;
                    break;
                }
            }

            if (!sawOverlapBegin)
            {
                ME_CORE_ERROR("PhysicsContactTest: expected Overlap Begin with Trigger.");
                return false;
            }

            if (!sawOverlapEnd)
            {
                ME_CORE_ERROR("PhysicsContactTest: expected Overlap End after leaving Trigger.");
                return false;
            }

            // Sensor must not stop the falling body near the trigger center (y≈5).
            const float finalHeight = dynamicRoot->GetPosition().y;
            if (!(finalHeight < heightAtBegin - 0.5f))
            {
                ME_CORE_ERROR(
                    "PhysicsContactTest: Trigger appears to block motion (beginY={}, finalY={}).",
                    heightAtBegin,
                    finalHeight);
                return false;
            }

            ME_CORE_INFO(
                "PhysicsContactTest: Overlap Begin/End observed (beginY={}, finalY={}).",
                heightAtBegin,
                finalHeight);
            return true;
        }

        bool RunChannelRegistryNameTest()
        {
            CollisionChannelRegistry& registry = CollisionChannelRegistry::Get();
            registry.ResetToDefaults();

            ECollisionChannel channel = ECollisionChannel::MAX;
            if (!registry.TryFindChannelByName("WorldStatic", channel)
                || channel != ECollisionChannel::WorldStatic)
            {
                ME_CORE_ERROR("PhysicsContactTest: WorldStatic name lookup failed.");
                return false;
            }

            if (registry.GetResponse(ECollisionChannel::Trigger, ECollisionChannel::Trigger)
                != ECollisionResponse::Ignore)
            {
                ME_CORE_ERROR("PhysicsContactTest: Trigger↔Trigger should Ignore.");
                return false;
            }

            if (registry.GetResponse(ECollisionChannel::Default, ECollisionChannel::Trigger)
                != ECollisionResponse::Overlap)
            {
                ME_CORE_ERROR("PhysicsContactTest: Default↔Trigger should Overlap.");
                return false;
            }

            return true;
        }
    } // namespace

    bool RunPhysicsContactTests()
    {
        return RunChannelRegistryNameTest()
            && RunBlockBeginTest()
            && RunOverlapBeginEndTest();
    }
}

#include "doctest.h"

#include "EngineTestFixture.h"

TEST_CASE("physics-contact: channel matrix and contact events [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunPhysicsContactTests());
}
