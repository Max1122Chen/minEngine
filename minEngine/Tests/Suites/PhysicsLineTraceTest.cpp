#include "PhysicsLineTraceTest.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Physics/BoxColliderComponent.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"
#include "Runtime/Function/Physics/PhysicsTypes.h"
#include "Runtime/Function/Physics/RigidBodyComponent.h"

#include <cmath>

namespace minEngine
{
    class PhysicsLineTraceTestScope
    {
    public:
        PhysicsLineTraceTestScope()
        {
            ObjectManager::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();

            SceneManager::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            PhysicsSystem::SetInstance(&m_PhysicsSystem);
            m_PhysicsSystem.Initialize();
        }

        ~PhysicsLineTraceTestScope()
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
        bool NearlyEqual(float a, float b, float epsilon = 0.15f)
        {
            return std::fabs(a - b) <= epsilon;
        }

        bool RunHitFloorTest()
        {
            PhysicsLineTraceTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("physics-linetrace-hit");
            if (!scene)
            {
                ME_CORE_ERROR("PhysicsLineTraceTest: failed to create hit scene.");
                return false;
            }

            const std::shared_ptr<GameObject> floorObject = scene->CreateGameObject();
            floorObject->AddComponent<SceneComponent>()->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
            const std::shared_ptr<RigidBodyComponent> floorRigidBody = floorObject->AddComponent<RigidBodyComponent>();
            floorRigidBody->SetBodyType(EBodyType::Static);
            const std::shared_ptr<BoxColliderComponent> floorCollider = floorObject->AddComponent<BoxColliderComponent>();
            floorCollider->SetObjectChannel(ECollisionChannel::WorldStatic);
            floorCollider->SetHalfExtent(Vector3(5.0f, 0.5f, 5.0f));

            HitResult hitResult;
            const bool hit = scene->LineTrace(
                Vector3(0.0f, 10.0f, 0.0f),
                Vector3(0.0f, -10.0f, 0.0f),
                ECollisionChannel::Visibility,
                CollisionQueryParams{},
                hitResult);

            if (!hit || !hitResult.bHit || !hitResult.bBlockingHit)
            {
                ME_CORE_ERROR("PhysicsLineTraceTest: expected blocking Visibility hit on WorldStatic floor.");
                return false;
            }

            if (hitResult.HitObject != floorObject.get() || hitResult.RigidBody != floorRigidBody.get())
            {
                ME_CORE_ERROR("PhysicsLineTraceTest: hit object/component mismatch.");
                return false;
            }

            // Floor top surface is at y = 0.5 with center at origin and half-extent 0.5.
            if (!NearlyEqual(hitResult.Location.y, 0.5f))
            {
                ME_CORE_ERROR(
                    "PhysicsLineTraceTest: unexpected hit Y {} (expected ~0.5).",
                    hitResult.Location.y);
                return false;
            }

            ME_CORE_INFO("PhysicsLineTraceTest: floor hit at Y={}.", hitResult.Location.y);
            return true;
        }

        bool RunMissTest()
        {
            PhysicsLineTraceTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("physics-linetrace-miss");
            if (!scene)
            {
                ME_CORE_ERROR("PhysicsLineTraceTest: failed to create miss scene.");
                return false;
            }

            const std::shared_ptr<GameObject> floorObject = scene->CreateGameObject();
            floorObject->AddComponent<SceneComponent>();
            const std::shared_ptr<RigidBodyComponent> floorRigidBody = floorObject->AddComponent<RigidBodyComponent>();
            floorRigidBody->SetBodyType(EBodyType::Static);
            const std::shared_ptr<BoxColliderComponent> floorCollider = floorObject->AddComponent<BoxColliderComponent>();
            floorCollider->SetObjectChannel(ECollisionChannel::WorldStatic);
            floorCollider->SetHalfExtent(Vector3(1.0f, 0.5f, 1.0f));

            HitResult hitResult;
            const bool hit = scene->LineTrace(
                Vector3(50.0f, 10.0f, 0.0f),
                Vector3(50.0f, -10.0f, 0.0f),
                ECollisionChannel::Default,
                CollisionQueryParams{},
                hitResult);

            if (hit || hitResult.bHit)
            {
                ME_CORE_ERROR("PhysicsLineTraceTest: expected miss far from floor.");
                return false;
            }

            return true;
        }

        bool RunIgnoreSelfTest()
        {
            PhysicsLineTraceTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("physics-linetrace-ignore");
            if (!scene)
            {
                ME_CORE_ERROR("PhysicsLineTraceTest: failed to create ignore scene.");
                return false;
            }

            const std::shared_ptr<GameObject> floorObject = scene->CreateGameObject();
            floorObject->AddComponent<SceneComponent>();
            const std::shared_ptr<RigidBodyComponent> floorRigidBody = floorObject->AddComponent<RigidBodyComponent>();
            floorRigidBody->SetBodyType(EBodyType::Static);
            const std::shared_ptr<BoxColliderComponent> floorCollider = floorObject->AddComponent<BoxColliderComponent>();
            floorCollider->SetObjectChannel(ECollisionChannel::WorldStatic);
            floorCollider->SetHalfExtent(Vector3(5.0f, 0.5f, 5.0f));

            const std::shared_ptr<GameObject> boxObject = scene->CreateGameObject();
            boxObject->AddComponent<SceneComponent>()->SetPosition(Vector3(0.0f, 3.0f, 0.0f));
            const std::shared_ptr<RigidBodyComponent> boxRigidBody = boxObject->AddComponent<RigidBodyComponent>();
            boxRigidBody->SetBodyType(EBodyType::Static);
            boxObject->AddComponent<BoxColliderComponent>()->SetObjectChannel(ECollisionChannel::Default);

            CollisionQueryParams params;
            params.IgnoreGameObject = boxObject.get();

            HitResult hitResult;
            const bool hit = scene->LineTrace(
                Vector3(0.0f, 3.0f, 0.0f),
                Vector3(0.0f, -10.0f, 0.0f),
                ECollisionChannel::Default,
                params,
                hitResult);

            if (!hit || hitResult.HitObject != floorObject.get())
            {
                ME_CORE_ERROR("PhysicsLineTraceTest: ignore-self should hit floor, not the box.");
                return false;
            }

            return true;
        }

        bool RunTriggerOverlapHitTest()
        {
            PhysicsLineTraceTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("physics-linetrace-trigger");
            if (!scene)
            {
                ME_CORE_ERROR("PhysicsLineTraceTest: failed to create trigger scene.");
                return false;
            }

            const std::shared_ptr<GameObject> triggerObject = scene->CreateGameObject();
            triggerObject->AddComponent<SceneComponent>()->SetPosition(Vector3(0.0f, 2.0f, 0.0f));
            const std::shared_ptr<RigidBodyComponent> triggerRigidBody = triggerObject->AddComponent<RigidBodyComponent>();
            triggerRigidBody->SetBodyType(EBodyType::Static);
            const std::shared_ptr<BoxColliderComponent> triggerCollider = triggerObject->AddComponent<BoxColliderComponent>();
            triggerCollider->SetObjectChannel(ECollisionChannel::Trigger);
            triggerCollider->SetHalfExtent(Vector3(1.0f, 1.0f, 1.0f));

            HitResult hitResult;
            const bool hit = scene->LineTrace(
                Vector3(0.0f, 10.0f, 0.0f),
                Vector3(0.0f, -10.0f, 0.0f),
                ECollisionChannel::Default,
                CollisionQueryParams{},
                hitResult);

            if (!hit || !hitResult.bHit || hitResult.bBlockingHit)
            {
                ME_CORE_ERROR(
                    "PhysicsLineTraceTest: Default×Trigger should Overlap-hit (bBlockingHit=false). hit={} blocking={}",
                    hit,
                    hitResult.bBlockingHit);
                return false;
            }

            if (hitResult.HitObject != triggerObject.get())
            {
                ME_CORE_ERROR("PhysicsLineTraceTest: trigger hit object mismatch.");
                return false;
            }

            return true;
        }

        bool RunVisibilityVsDefaultTest()
        {
            PhysicsLineTraceTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("physics-linetrace-visibility");
            if (!scene)
            {
                ME_CORE_ERROR("PhysicsLineTraceTest: failed to create visibility scene.");
                return false;
            }

            const std::shared_ptr<GameObject> boxObject = scene->CreateGameObject();
            boxObject->AddComponent<SceneComponent>()->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
            const std::shared_ptr<RigidBodyComponent> boxRigidBody = boxObject->AddComponent<RigidBodyComponent>();
            boxRigidBody->SetBodyType(EBodyType::Static);
            boxObject->AddComponent<BoxColliderComponent>()->SetObjectChannel(ECollisionChannel::Default);

            HitResult hitResult;
            const bool hit = scene->LineTrace(
                Vector3(0.0f, 5.0f, 0.0f),
                Vector3(0.0f, -5.0f, 0.0f),
                ECollisionChannel::Visibility,
                CollisionQueryParams{},
                hitResult);

            if (!hit || !hitResult.bBlockingHit || hitResult.HitObject != boxObject.get())
            {
                ME_CORE_ERROR("PhysicsLineTraceTest: Visibility×Default should blocking-hit.");
                return false;
            }

            return true;
        }
    } // namespace

    bool RunPhysicsLineTraceTests()
    {
        return RunHitFloorTest()
            && RunMissTest()
            && RunIgnoreSelfTest()
            && RunTriggerOverlapHitTest()
            && RunVisibilityVsDefaultTest();
    }
}

#include "doctest.h"

#include "EngineTestFixture.h"

TEST_CASE("physics-linetrace: Scene LineTrace queries [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunPhysicsLineTraceTests());
}
