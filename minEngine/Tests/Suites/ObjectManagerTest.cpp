#include "ObjectManager.h"

#include "Runtime/Function/Framework/Components/MovementComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

#include "doctest.h"

#include "EngineTestFixture.h"

namespace minEngine
{
    class ObjectManagerTestScope
    {
    public:
        ObjectManagerTestScope()
        {
            ObjectManager::SetInstance(&m_Manager);
            m_Manager.Initialize();
        }

        ~ObjectManagerTestScope()
        {
            m_Manager.Shutdown();
            ObjectManager::SetInstance(nullptr);
        }

    private:
        ObjectManager m_Manager;
    };
}

TEST_CASE("object-manager: shared_ptr unregister [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::ObjectManagerTestScope scope;

    minEngine::GUID guid;
    {
        std::shared_ptr<minEngine::Scene> scene = minEngine::NewObject<minEngine::Scene>("TransientScene");
        guid = scene->GetGuid();
        CHECK(minEngine::FindObject(guid) == scene);
        CHECK(minEngine::ObjectManager::Get().GetTrackedObjectCount() == 1);
    }

    CHECK(minEngine::FindObject(guid) == nullptr);
    minEngine::ObjectManager::Get().CollectGarbage();
    CHECK(minEngine::ObjectManager::Get().GetTrackedObjectCount() == 0);
}

TEST_CASE("object-manager: hierarchy teardown [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::ObjectManagerTestScope scope;

    minEngine::GUID sceneGuid;
    minEngine::GUID gameObjectGuid;
    minEngine::GUID componentGuid;

    {
        std::shared_ptr<minEngine::Scene> scene = minEngine::NewObject<minEngine::Scene>("HierarchyScene");
        sceneGuid = scene->GetGuid();

        std::shared_ptr<minEngine::GameObject> gameObject = scene->CreateGameObject();
        gameObjectGuid = gameObject->GetGuid();

        std::shared_ptr<minEngine::MovementComponent> movement = gameObject->AddComponent<minEngine::MovementComponent>();
        componentGuid = movement->GetGuid();

        CHECK(minEngine::ObjectManager::Get().GetTrackedObjectCount() >= 3);
    }

    CHECK(minEngine::FindObject(sceneGuid) == nullptr);
    CHECK(minEngine::FindObject(gameObjectGuid) == nullptr);
    CHECK(minEngine::FindObject(componentGuid) == nullptr);

    minEngine::ObjectManager::Get().CollectGarbage();
    CHECK(minEngine::ObjectManager::Get().GetTrackedObjectCount() == 0);
}

TEST_CASE("object-manager: engine roots GC [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::ObjectManagerTestScope scope;

    std::shared_ptr<minEngine::Scene> scene = minEngine::NewObject<minEngine::Scene>("EngineRootScene");
    scene->CreateGameObject();

    minEngine::ObjectManager::Get().RegisterGarbageRootSource(
        scene.get(),
        [&scene](const minEngine::ObjectReachabilityMarker& markReachable) {
            scene->MarkReachableObjects(markReachable);
        });

    minEngine::ObjectManager::Get().CollectGarbageWithEngineRoots();
    minEngine::ObjectManager::Get().UnregisterGarbageRootSource(scene.get());
    CHECK(minEngine::ObjectManager::Get().GetTrackedObjectCount() >= 1);
}
