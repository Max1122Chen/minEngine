#include "PhysicsLoadTest.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Physics/BoxColliderComponent.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"
#include "Runtime/Function/Physics/RigidBodyComponent.h"

#include <cmath>

namespace minEngine
{
    class PhysicsLoadTestScope
    {
    public:
        PhysicsLoadTestScope()
        {
            ObjectManager::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();

            SceneManager::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            PhysicsSystem::SetInstance(&m_PhysicsSystem);
            m_PhysicsSystem.Initialize();
        }

        ~PhysicsLoadTestScope()
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

        bool RunRebuildWorldBodiesAfterEmptyWorldTest()
        {
            PhysicsLoadTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("physics-load");
            if (!scene)
            {
                ME_CORE_ERROR("PhysicsLoadTest: failed to create scene.");
                return false;
            }

            const std::shared_ptr<GameObject> floorObject = scene->CreateGameObject();
            floorObject->AddComponent<SceneComponent>();
            const std::shared_ptr<RigidBodyComponent> floorRigidBody = floorObject->AddComponent<RigidBodyComponent>();
            floorRigidBody->SetBodyType(EBodyType::Static);
            floorObject->AddComponent<BoxColliderComponent>();

            const std::shared_ptr<GameObject> dynamicObject = scene->CreateGameObject();
            const std::shared_ptr<SceneComponent> dynamicRoot = dynamicObject->AddComponent<SceneComponent>();
            dynamicRoot->SetPosition(Vector3(0.0f, 10.0f, 0.0f));
            const std::shared_ptr<RigidBodyComponent> dynamicRigidBody = dynamicObject->AddComponent<RigidBodyComponent>();
            dynamicObject->AddComponent<BoxColliderComponent>();

            if (!dynamicRigidBody->HasValidPhysicsBody())
            {
                ME_CORE_ERROR("PhysicsLoadTest: dynamic body missing after setup.");
                return false;
            }

            PhysicsSystem::Get().DestroyWorld(scene.get());
            PhysicsSystem::Get().GetOrCreateWorld(scene.get());
            PhysicsSystem::Get().RebuildWorldBodies(scene.get());

            if (!dynamicRigidBody->HasValidPhysicsBody())
            {
                ME_CORE_ERROR("PhysicsLoadTest: rebuild did not register dynamic body.");
                return false;
            }

            for (int stepIndex = 0; stepIndex < 90; ++stepIndex)
            {
                (void)stepIndex;
                PhysicsSystem::Get().SimulateActiveScene(FixedDeltaTime);
            }

            const float finalHeight = dynamicRoot->GetPosition().y;
            if (!(finalHeight < 10.0f && finalHeight > 0.5f))
            {
                ME_CORE_ERROR("PhysicsLoadTest: rebuild world bodies did not simulate (height={}).", finalHeight);
                return false;
            }

            ME_CORE_INFO("PhysicsLoadTest: rebuild world bodies fell to {}.", finalHeight);
            return true;
        }
    } // namespace

    bool RunPhysicsLoadTests()
    {
        return RunRebuildWorldBodiesAfterEmptyWorldTest();
    }
}

#include "doctest.h"

#include "EngineTestFixture.h"

TEST_CASE("physics-load: rebuild world bodies after empty physics world [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunPhysicsLoadTests());
}
