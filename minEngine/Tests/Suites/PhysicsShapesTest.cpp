#include "PhysicsShapesTest.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Physics/BoxColliderComponent.h"
#include "Runtime/Function/Physics/CapsuleColliderComponent.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"
#include "Runtime/Function/Physics/PhysicsTypes.h"
#include "Runtime/Function/Physics/RigidBodyComponent.h"
#include "Runtime/Function/Physics/SphereColliderComponent.h"

namespace minEngine
{
    class PhysicsShapesTestScope
    {
    public:
        PhysicsShapesTestScope()
        {
            ObjectManager::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();
            SceneManager::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
            PhysicsSystem::SetInstance(&m_PhysicsSystem);
            m_PhysicsSystem.Initialize();
        }

        ~PhysicsShapesTestScope()
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
        bool RunSphereFallTest()
        {
            PhysicsShapesTestScope scope;
            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("physics-shapes-sphere");
            if (!scene)
            {
                return false;
            }

            const std::shared_ptr<GameObject> floorObject = scene->CreateGameObject();
            floorObject->AddComponent<SceneComponent>();
            floorObject->AddComponent<RigidBodyComponent>()->SetBodyType(EBodyType::Static);
            auto floorCollider = floorObject->AddComponent<BoxColliderComponent>();
            floorCollider->SetObjectChannel(ECollisionChannel::WorldStatic);
            floorCollider->SetHalfExtent(Vector3(5.0f, 0.5f, 5.0f));

            const std::shared_ptr<GameObject> ballObject = scene->CreateGameObject();
            ballObject->AddComponent<SceneComponent>()->SetPosition(Vector3(0.0f, 8.0f, 0.0f));
            ballObject->AddComponent<RigidBodyComponent>()->SetBodyType(EBodyType::Dynamic);
            ballObject->AddComponent<SphereColliderComponent>()->SetRadius(0.5f);

            constexpr float dt = 1.0f / 60.0f;
            for (int i = 0; i < 120; ++i)
            {
                PhysicsSystem::Get().SimulateActiveScene(dt);
            }

            const float y = ballObject->GetPosition().y;
            if (!(y < 8.0f && y > 0.4f))
            {
                ME_CORE_ERROR("PhysicsShapesTest: sphere unexpected Y={}", y);
                return false;
            }

            ME_CORE_INFO("PhysicsShapesTest: sphere rested at Y={}.", y);
            return true;
        }

        bool RunCapsuleFallTest()
        {
            PhysicsShapesTestScope scope;
            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("physics-shapes-capsule");
            if (!scene)
            {
                return false;
            }

            const std::shared_ptr<GameObject> floorObject = scene->CreateGameObject();
            floorObject->AddComponent<SceneComponent>();
            floorObject->AddComponent<RigidBodyComponent>()->SetBodyType(EBodyType::Static);
            auto floorCollider = floorObject->AddComponent<BoxColliderComponent>();
            floorCollider->SetObjectChannel(ECollisionChannel::WorldStatic);
            floorCollider->SetHalfExtent(Vector3(5.0f, 0.5f, 5.0f));

            const std::shared_ptr<GameObject> capsuleObject = scene->CreateGameObject();
            capsuleObject->AddComponent<SceneComponent>()->SetPosition(Vector3(0.0f, 8.0f, 0.0f));
            capsuleObject->AddComponent<RigidBodyComponent>()->SetBodyType(EBodyType::Dynamic);
            auto capsule = capsuleObject->AddComponent<CapsuleColliderComponent>();
            capsule->SetRadius(0.4f);
            capsule->SetHalfHeight(0.4f);

            constexpr float dt = 1.0f / 60.0f;
            for (int i = 0; i < 120; ++i)
            {
                PhysicsSystem::Get().SimulateActiveScene(dt);
            }

            const float y = capsuleObject->GetPosition().y;
            if (!(y < 8.0f && y > 0.5f))
            {
                ME_CORE_ERROR("PhysicsShapesTest: capsule unexpected Y={}", y);
                return false;
            }

            ME_CORE_INFO("PhysicsShapesTest: capsule rested at Y={}.", y);
            return true;
        }

        bool RunSphereTraceHitTest()
        {
            PhysicsShapesTestScope scope;
            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("physics-shapes-spheretrace");
            if (!scene)
            {
                return false;
            }

            const std::shared_ptr<GameObject> boxObject = scene->CreateGameObject();
            boxObject->AddComponent<SceneComponent>()->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
            boxObject->AddComponent<RigidBodyComponent>()->SetBodyType(EBodyType::Static);
            boxObject->AddComponent<BoxColliderComponent>()->SetObjectChannel(ECollisionChannel::Default);

            HitResult hit;
            const bool ok = scene->SphereTrace(
                Vector3(0.0f, 5.0f, 0.0f),
                Vector3(0.0f, -5.0f, 0.0f),
                0.25f,
                ECollisionChannel::Visibility,
                CollisionQueryParams{},
                hit);

            if (!ok || !hit.bBlockingHit || hit.HitObject != boxObject.get())
            {
                ME_CORE_ERROR("PhysicsShapesTest: SphereTrace expected blocking hit.");
                return false;
            }

            return true;
        }

        bool RunCapsuleTraceMissAndIgnoreTest()
        {
            PhysicsShapesTestScope scope;
            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("physics-shapes-capsuletrace");
            if (!scene)
            {
                return false;
            }

            const std::shared_ptr<GameObject> floorObject = scene->CreateGameObject();
            floorObject->AddComponent<SceneComponent>();
            floorObject->AddComponent<RigidBodyComponent>()->SetBodyType(EBodyType::Static);
            auto floorCollider = floorObject->AddComponent<BoxColliderComponent>();
            floorCollider->SetObjectChannel(ECollisionChannel::WorldStatic);
            floorCollider->SetHalfExtent(Vector3(5.0f, 0.5f, 5.0f));

            const std::shared_ptr<GameObject> selfObject = scene->CreateGameObject();
            selfObject->AddComponent<SceneComponent>()->SetPosition(Vector3(0.0f, 3.0f, 0.0f));
            selfObject->AddComponent<RigidBodyComponent>()->SetBodyType(EBodyType::Static);
            selfObject->AddComponent<SphereColliderComponent>()->SetRadius(0.5f);

            HitResult missHit;
            if (scene->CapsuleTrace(
                    Vector3(40.0f, 5.0f, 0.0f),
                    Vector3(40.0f, -5.0f, 0.0f),
                    0.3f,
                    0.3f,
                    ECollisionChannel::Default,
                    CollisionQueryParams{},
                    missHit))
            {
                ME_CORE_ERROR("PhysicsShapesTest: CapsuleTrace expected miss far away.");
                return false;
            }

            CollisionQueryParams ignoreParams;
            ignoreParams.IgnoreGameObject = selfObject.get();
            HitResult hit;
            if (!scene->CapsuleTrace(
                    Vector3(0.0f, 3.0f, 0.0f),
                    Vector3(0.0f, -10.0f, 0.0f),
                    0.3f,
                    0.3f,
                    ECollisionChannel::Default,
                    ignoreParams,
                    hit)
                || hit.HitObject != floorObject.get())
            {
                ME_CORE_ERROR("PhysicsShapesTest: CapsuleTrace ignore-self should hit floor.");
                return false;
            }

            return true;
        }
    }

    bool RunPhysicsShapesTests()
    {
        return RunSphereFallTest()
            && RunCapsuleFallTest()
            && RunSphereTraceHitTest()
            && RunCapsuleTraceMissAndIgnoreTest();
    }
}

#include "doctest.h"
#include "EngineTestFixture.h"

TEST_CASE("physics-shapes: colliders and shape traces [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunPhysicsShapesTests());
}
