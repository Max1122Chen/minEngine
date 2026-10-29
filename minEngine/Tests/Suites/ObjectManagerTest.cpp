#include "ObjectManager.h"
#include "Access/ObjectManagerTestAccess.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Function/Framework/Components/MovementComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

#include "doctest.h"

#include <thread>

#include "EngineTestFixture.h"

namespace minEngine
{
    class ObjectManagerTestScope
    {
    public:
        ObjectManagerTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_Manager);
            ObjectManager::Initialize();
        }

        ~ObjectManagerTestScope()
        {
            ObjectManager::Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
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

TEST_CASE("object-manager: creating an object off the game role is counted and still creates [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::ObjectManagerTestScope scope;

    // Multi, so this thread owns the Game role and the worker below owns nothing: the wrong-thread
    // caller is a real one, not a simulated role.
    minEngine::FrameThreading threading;
    threading.Initialize(minEngine::FrameThreadMode::Multi);

    const uint64_t createsBefore = minEngine::ObjectManager::GetOffRoleCoreServiceCallCount(
        minEngine::ECoreServiceEntry::NewObject);
    const uint64_t violationsBefore = minEngine::GetContractViolationCounters().Total();

    minEngine::GUID createdGuid;
    // Held past the thread: the registry tracks live objects weakly, so a creation that nothing owns
    // would be gone before the lookup below and the check would test the wrong thing.
    std::shared_ptr<minEngine::Scene> created;
    std::thread worker([&created, &createdGuid]() {
        created = minEngine::NewObject<minEngine::Scene>("OffRoleScene");
        if (created)
        {
            createdGuid = created->GetGuid();
        }
    });
    worker.join();

    CHECK(minEngine::ObjectManager::GetOffRoleCoreServiceCallCount(
        minEngine::ECoreServiceEntry::NewObject) == createsBefore + 1);
    CHECK(minEngine::GetContractViolationCounters().Total() > violationsBefore);

    // Behaviour unchanged: the caller was reported, not refused, so the object exists and the game
    // role can still find it in the registry.
    REQUIRE_FALSE(createdGuid.IsZero());
    CHECK(minEngine::FindObject(createdGuid) != nullptr);

    threading.Shutdown();
}
