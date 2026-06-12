#include "PhysicsSmokeTest.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Physics/BoxColliderComponent.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"
#include "Runtime/Function/Physics/RigidBodyComponent.h"

namespace minEngine
{
    class PhysicsSmokeTestScope
    {
    public:
        PhysicsSmokeTestScope()
        {
            ObjectManager::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();

            SceneManager::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            PhysicsSystem::SetInstance(&m_PhysicsSystem);
            m_PhysicsSystem.Initialize();
        }

        ~PhysicsSmokeTestScope()
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
        bool RunPhysicsFallingBoxTest()
        {
            PhysicsSmokeTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("physics-smoke");
            if (!scene)
            {
                ME_CORE_ERROR("PhysicsSmokeTest: failed to create scene.");
                return false;
            }

            const std::shared_ptr<GameObject> floorObject = scene->CreateGameObject();
            floorObject->AddComponent<SceneComponent>();
            const std::shared_ptr<RigidBodyComponent> floorRigidBody = floorObject->AddComponent<RigidBodyComponent>();
            floorRigidBody->SetBodyType(EBodyType::Static);
            const std::shared_ptr<BoxColliderComponent> floorCollider = floorObject->AddComponent<BoxColliderComponent>();
            floorCollider->SetHalfExtent(Vector3(5.0f, 0.5f, 5.0f));

            const std::shared_ptr<GameObject> dynamicObject = scene->CreateGameObject();
            const std::shared_ptr<SceneComponent> dynamicRoot = dynamicObject->AddComponent<SceneComponent>();
            dynamicRoot->SetPosition(Vector3(0.0f, 10.0f, 0.0f));
            const std::shared_ptr<RigidBodyComponent> dynamicRigidBody = dynamicObject->AddComponent<RigidBodyComponent>();
            dynamicRigidBody->SetBodyType(EBodyType::Dynamic);
            dynamicRigidBody->SetMass(1.0f);
            dynamicObject->AddComponent<BoxColliderComponent>();

            if (!floorRigidBody->HasValidPhysicsBody() || !dynamicRigidBody->HasValidPhysicsBody())
            {
                ME_CORE_ERROR("PhysicsSmokeTest: rigid bodies were not created.");
                return false;
            }

            constexpr float fixedDeltaTime = 1.0f / 60.0f;
            constexpr int simulationSteps = 90;
            for (int stepIndex = 0; stepIndex < simulationSteps; ++stepIndex)
            {
                (void)stepIndex;
                PhysicsSystem::Get().SimulateActiveScene(fixedDeltaTime);
            }

            const float finalHeight = dynamicObject->GetPosition().y;
            if (!(finalHeight < 10.0f && finalHeight > 0.5f))
            {
                ME_CORE_ERROR(
                    "PhysicsSmokeTest: unexpected dynamic box height {} (expected < 10 and > 0.5).",
                    finalHeight);
                return false;
            }

            ME_CORE_INFO("PhysicsSmokeTest: dynamic box fell to Y={}.", finalHeight);
            return true;
        }
    } // namespace

    bool RunPhysicsSmokeTests()
    {
        return RunPhysicsFallingBoxTest();
    }
}

#include "doctest.h"

#include "EngineTestFixture.h"

TEST_CASE("physics-smoke: dynamic box falls onto static floor [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunPhysicsSmokeTests());
}
