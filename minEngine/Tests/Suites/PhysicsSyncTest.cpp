#include "PhysicsSyncTest.h"

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
    class PhysicsSyncTestScope
    {
    public:
        PhysicsSyncTestScope()
        {
            ObjectManager::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();

            SceneManager::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            PhysicsSystem::SetInstance(&m_PhysicsSystem);
            m_PhysicsSystem.Initialize();
        }

        ~PhysicsSyncTestScope()
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
        constexpr float FixedDeltaTime = 1.0f / 60.0f;

        std::shared_ptr<Scene> CreateSceneWithFloorAndDynamic(
            std::shared_ptr<RigidBodyComponent>& outDynamicRigidBody,
            std::shared_ptr<SceneComponent>& outDynamicRoot)
        {
            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("physics-sync");
            if (!scene)
            {
                return nullptr;
            }

            const std::shared_ptr<GameObject> floorObject = scene->CreateGameObject();
            floorObject->AddComponent<SceneComponent>();
            const std::shared_ptr<RigidBodyComponent> floorRigidBody = floorObject->AddComponent<RigidBodyComponent>();
            floorRigidBody->SetBodyType(EBodyType::Static);
            const std::shared_ptr<BoxColliderComponent> floorCollider = floorObject->AddComponent<BoxColliderComponent>();
            floorCollider->SetHalfExtent(Vector3(5.0f, 0.5f, 5.0f));

            const std::shared_ptr<GameObject> dynamicObject = scene->CreateGameObject();
            outDynamicRoot = dynamicObject->AddComponent<SceneComponent>();
            outDynamicRoot->SetPosition(Vector3(0.0f, 10.0f, 0.0f));
            outDynamicRigidBody = dynamicObject->AddComponent<RigidBodyComponent>();
            outDynamicRigidBody->SetBodyType(EBodyType::Dynamic);
            outDynamicRigidBody->SetMass(1.0f);
            dynamicObject->AddComponent<BoxColliderComponent>();

            if (!floorRigidBody->HasValidPhysicsBody() || !outDynamicRigidBody->HasValidPhysicsBody())
            {
                return nullptr;
            }

            return scene;
        }

        bool RunResetTeleportWhileSimulatingTest()
        {
            PhysicsSyncTestScope scope;

            std::shared_ptr<RigidBodyComponent> dynamicRigidBody;
            std::shared_ptr<SceneComponent> dynamicRoot;
            if (!CreateSceneWithFloorAndDynamic(dynamicRigidBody, dynamicRoot))
            {
                ME_CORE_ERROR("PhysicsSyncTest: failed to create scene for reset teleport.");
                return false;
            }

            for (int stepIndex = 0; stepIndex < 30; ++stepIndex)
            {
                (void)stepIndex;
                PhysicsSystem::Get().SimulateActiveScene(FixedDeltaTime);
            }

            const float heightBeforeTeleport = dynamicRoot->GetPosition().y;
            dynamicRoot->SetPosition(Vector3(0.0f, 15.0f, 0.0f), ETeleportType::ResetPhysics);
            PhysicsSystem::Get().SimulateActiveScene(FixedDeltaTime);

            const float heightAfterTeleport = dynamicRoot->GetPosition().y;
            if (!(heightAfterTeleport > heightBeforeTeleport && heightAfterTeleport > 14.5f && heightAfterTeleport <= 15.05f))
            {
                ME_CORE_ERROR(
                    "PhysicsSyncTest: reset teleport did not reseed height (before={}, after={}).",
                    heightBeforeTeleport,
                    heightAfterTeleport);
                return false;
            }

            ME_CORE_INFO("PhysicsSyncTest: reset teleport reseeded height to {}.", heightAfterTeleport);
            return true;
        }

        bool RunSimulatePhysicsOffTest()
        {
            PhysicsSyncTestScope scope;

            std::shared_ptr<RigidBodyComponent> dynamicRigidBody;
            std::shared_ptr<SceneComponent> dynamicRoot;
            if (!CreateSceneWithFloorAndDynamic(dynamicRigidBody, dynamicRoot))
            {
                ME_CORE_ERROR("PhysicsSyncTest: failed to create scene for simulate off.");
                return false;
            }

            dynamicRigidBody->SetSimulatePhysics(false);
            const float initialHeight = dynamicRoot->GetPosition().y;

            for (int stepIndex = 0; stepIndex < 90; ++stepIndex)
            {
                (void)stepIndex;
                PhysicsSystem::Get().SimulateActiveScene(FixedDeltaTime);
            }

            const float finalHeight = dynamicRoot->GetPosition().y;
            if (std::fabs(finalHeight - initialHeight) > 0.01f)
            {
                ME_CORE_ERROR(
                    "PhysicsSyncTest: simulate off allowed movement (initial={}, final={}).",
                    initialHeight,
                    finalHeight);
                return false;
            }

            ME_CORE_INFO("PhysicsSyncTest: simulate off held height at {}.", finalHeight);
            return true;
        }

        bool RunSimulatePhysicsOnAfterOffTest()
        {
            PhysicsSyncTestScope scope;

            std::shared_ptr<RigidBodyComponent> dynamicRigidBody;
            std::shared_ptr<SceneComponent> dynamicRoot;
            if (!CreateSceneWithFloorAndDynamic(dynamicRigidBody, dynamicRoot))
            {
                ME_CORE_ERROR("PhysicsSyncTest: failed to create scene for simulate on.");
                return false;
            }

            dynamicRigidBody->SetSimulatePhysics(false);
            for (int stepIndex = 0; stepIndex < 90; ++stepIndex)
            {
                (void)stepIndex;
                PhysicsSystem::Get().SimulateActiveScene(FixedDeltaTime);
            }

            dynamicRigidBody->SetSimulatePhysics(true);
            for (int stepIndex = 0; stepIndex < 90; ++stepIndex)
            {
                (void)stepIndex;
                PhysicsSystem::Get().SimulateActiveScene(FixedDeltaTime);
            }

            const float finalHeight = dynamicRoot->GetPosition().y;
            if (!(finalHeight < 10.0f && finalHeight > 0.5f))
            {
                ME_CORE_ERROR(
                    "PhysicsSyncTest: simulate on after off produced unexpected height {}.",
                    finalHeight);
                return false;
            }

            ME_CORE_INFO("PhysicsSyncTest: simulate on after off fell to {}.", finalHeight);
            return true;
        }

        bool RunTeleportPhysicsPreservesVelocityTest()
        {
            PhysicsSyncTestScope scope;

            std::shared_ptr<RigidBodyComponent> dynamicRigidBody;
            std::shared_ptr<SceneComponent> dynamicRoot;
            if (!CreateSceneWithFloorAndDynamic(dynamicRigidBody, dynamicRoot))
            {
                ME_CORE_ERROR("PhysicsSyncTest: failed to create scene for teleport preserve velocity.");
                return false;
            }

            for (int stepIndex = 0; stepIndex < 30; ++stepIndex)
            {
                (void)stepIndex;
                PhysicsSystem::Get().SimulateActiveScene(FixedDeltaTime);
            }

            dynamicRoot->SetPosition(Vector3(0.0f, 8.0f, 0.0f), ETeleportType::ResetPhysics);
            PhysicsSystem::Get().SimulateActiveScene(FixedDeltaTime);
            const float resetHeightAfterOneStep = dynamicRoot->GetPosition().y;

            dynamicRoot->SetPosition(Vector3(0.0f, 10.0f, 0.0f), ETeleportType::ResetPhysics);
            for (int stepIndex = 0; stepIndex < 30; ++stepIndex)
            {
                (void)stepIndex;
                PhysicsSystem::Get().SimulateActiveScene(FixedDeltaTime);
            }

            dynamicRoot->SetPosition(Vector3(0.0f, 8.0f, 0.0f), ETeleportType::TeleportPhysics);
            PhysicsSystem::Get().SimulateActiveScene(FixedDeltaTime);
            const float preserveHeightAfterOneStep = dynamicRoot->GetPosition().y;

            if (preserveHeightAfterOneStep >= resetHeightAfterOneStep)
            {
                ME_CORE_ERROR(
                    "PhysicsSyncTest: TeleportPhysics did not preserve fall speed (resetStep={}, preserveStep={}).",
                    resetHeightAfterOneStep,
                    preserveHeightAfterOneStep);
                return false;
            }

            ME_CORE_INFO(
                "PhysicsSyncTest: TeleportPhysics preserved velocity (resetStep={}, preserveStep={}).",
                resetHeightAfterOneStep,
                preserveHeightAfterOneStep);
            return true;
        }
    } // namespace

    bool RunPhysicsSyncTests()
    {
        return RunResetTeleportWhileSimulatingTest()
            && RunSimulatePhysicsOffTest()
            && RunSimulatePhysicsOnAfterOffTest()
            && RunTeleportPhysicsPreservesVelocityTest();
    }
}

#include "doctest.h"

#include "EngineTestFixture.h"

TEST_CASE("physics-sync: scene authority teleport and simulate gate [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunPhysicsSyncTests());
}
