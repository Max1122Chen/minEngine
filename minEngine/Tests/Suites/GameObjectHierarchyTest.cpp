#include "GameObjectHierarchyTest.h"

#include "EngineTestFixture.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Core/Serialization/Serializer.h"

#include "doctest.h"

namespace minEngine
{
    class GameObjectHierarchyTestScope
    {
    public:
        GameObjectHierarchyTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();
            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~GameObjectHierarchyTestScope()
        {
            m_SceneManager.Shutdown();
            Testing::TestAccess<SceneManager>::SetInstance(nullptr);
            m_ObjectManager.Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
    };
}

TEST_CASE("gameobject-hierarchy: attach detach and cycle [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    GameObjectHierarchyTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("go-hierarchy");
    REQUIRE(scene);

    const std::shared_ptr<GameObject> parent = scene->CreateGameObject();
    const std::shared_ptr<GameObject> child = scene->CreateGameObject();
    parent->AddComponent<SceneComponent>();
    child->AddComponent<SceneComponent>();

    CHECK(child->AttachToParent(parent.get(), AttachmentTransformRules::KeepRelativeTransform));
    CHECK(child->GetParent() == parent.get());
    REQUIRE(parent->GetChildren().size() == 1);
    CHECK(parent->GetChildren()[0] == child.get());
    CHECK(child->GetRootComponent()->GetAttachParent() == parent->GetRootComponent());

    CHECK_FALSE(parent->AttachToParent(child.get(), AttachmentTransformRules::KeepRelativeTransform));

    child->DetachFromParent(AttachmentTransformRules::KeepWorldTransform);
    CHECK(child->GetParent() == nullptr);
    CHECK(parent->GetChildren().empty());
}

TEST_CASE("gameobject-hierarchy: cascade remove [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    GameObjectHierarchyTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("go-cascade");
    const std::shared_ptr<GameObject> parent = scene->CreateGameObject();
    const std::shared_ptr<GameObject> child = scene->CreateGameObject();
    const std::shared_ptr<GameObject> grand = scene->CreateGameObject();
    parent->AddComponent<SceneComponent>();
    child->AddComponent<SceneComponent>();
    grand->AddComponent<SceneComponent>();

    CHECK(child->AttachToParent(parent.get(), AttachmentTransformRules::KeepRelativeTransform));
    CHECK(grand->AttachToParent(child.get(), AttachmentTransformRules::KeepRelativeTransform));

    const uint64_t parentId = parent->GetID();
    const uint64_t childId = child->GetID();
    const uint64_t grandId = grand->GetID();
    CHECK(scene->RemoveGameObjectById(parentId));
    CHECK(scene->FindGameObjectById(parentId) == nullptr);
    CHECK(scene->FindGameObjectById(childId) == nullptr);
    CHECK(scene->FindGameObjectById(grandId) == nullptr);
}

TEST_CASE("gameobject-hierarchy: parent pointer serialize round-trip [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    GameObjectHierarchyTestScope scope;

    const std::shared_ptr<Scene> source = SceneManager::Get().CreateNewScene("go-hier-ser");
    const std::shared_ptr<GameObject> parent = source->CreateGameObject();
    const std::shared_ptr<GameObject> child = source->CreateGameObject();
    parent->AddComponent<SceneComponent>();
    child->AddComponent<SceneComponent>();
    REQUIRE(child->AttachToParent(parent.get(), AttachmentTransformRules::KeepRelativeTransform));

    const minEngine::GUID parentGuid = parent->GetGuid();
    const minEngine::GUID childGuid = child->GetGuid();

    std::vector<uint8_t> buffer;
    REQUIRE(Serialization::Serializer::SerializeObjectToBuffer("minEngine::Scene", source.get(), buffer).ok);

    std::shared_ptr<Scene> loaded = NewObject<Scene>();
    loaded->m_SceneName = "go-hier-ser-loaded";
    ObjectManager::Get().UnregisterObject(loaded.get());

    std::vector<Serialization::PendingObjectRef> refs;
    REQUIRE(Serialization::Serializer::DeserializeObjectFromBuffer(
                "minEngine::Scene", loaded.get(), buffer, refs)
                .ok);
    REQUIRE(Serialization::Serializer::ResolvePendingObjectRefs(refs).ok);

    ObjectManager::Get().RegisterObject(loaded);
    loaded->SetSceneType(ESceneType::Editor);
    SceneManager::FinalizeLoadedScene(loaded.get());

    GameObject* loadedParent = nullptr;
    GameObject* loadedChild = nullptr;
    for (const std::shared_ptr<GameObject>& go : loaded->GetAllGameObjects())
    {
        if (!go)
        {
            continue;
        }
        if (go->GetGuid() == parentGuid)
        {
            loadedParent = go.get();
        }
        else if (go->GetGuid() == childGuid)
        {
            loadedChild = go.get();
        }
    }

    REQUIRE(loadedParent != nullptr);
    REQUIRE(loadedChild != nullptr);
    CHECK(loadedChild->GetParent() == loadedParent);
    REQUIRE(loadedParent->GetChildren().size() == 1);
    CHECK(loadedParent->GetChildren()[0] == loadedChild);
    CHECK(loadedChild->GetRootComponent()->GetAttachParent() == loadedParent->GetRootComponent());
}


TEST_CASE("gameobject-hierarchy: keep world and parent drives child [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    GameObjectHierarchyTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("go-hier-xform");
    REQUIRE(scene);

    const std::shared_ptr<GameObject> parent = scene->CreateGameObject();
    const std::shared_ptr<GameObject> child = scene->CreateGameObject();
    parent->AddComponent<SceneComponent>();
    child->AddComponent<SceneComponent>();

    parent->GetRootComponent()->SetPosition(Vector3(10.0f, 0.0f, 0.0f));
    child->GetRootComponent()->SetPosition(Vector3(12.0f, 0.0f, 0.0f));

    REQUIRE(child->AttachToParent(parent.get(), AttachmentTransformRules::KeepWorldTransform));
    CHECK(child->GetParent() == parent.get());
    CHECK(child->GetRootComponent()->GetAttachParent() == parent->GetRootComponent());

    const Vector3 worldAfterAttach = child->GetRootComponent()->GetWorldPosition();
    CHECK(worldAfterAttach.x == doctest::Approx(12.0f).epsilon(0.001f));
    CHECK(worldAfterAttach.y == doctest::Approx(0.0f).epsilon(0.001f));
    CHECK(worldAfterAttach.z == doctest::Approx(0.0f).epsilon(0.001f));

    parent->GetRootComponent()->SetPosition(Vector3(20.0f, 0.0f, 0.0f));
    const Vector3 worldAfterParentMove = child->GetRootComponent()->GetWorldPosition();
    CHECK(worldAfterParentMove.x == doctest::Approx(22.0f).epsilon(0.001f));
    CHECK(worldAfterParentMove.y == doctest::Approx(0.0f).epsilon(0.001f));
    CHECK(worldAfterParentMove.z == doctest::Approx(0.0f).epsilon(0.001f));
}


TEST_CASE("gameobject-hierarchy: remove attached scene component cleans attach links [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    GameObjectHierarchyTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("go-hier-remove");
    REQUIRE(scene);

    const std::shared_ptr<GameObject> go = scene->CreateGameObject();
    const std::shared_ptr<SceneComponent> root = go->AddComponent<SceneComponent>();
    const std::shared_ptr<SceneComponent> child = go->AddComponent<SceneComponent>();
    REQUIRE(root);
    REQUIRE(child);
    REQUIRE(go->GetRootComponent() == root.get());
    REQUIRE(child->GetAttachParent() == root.get());

    child->MarkRenderStateDirty();
    REQUIRE(go->RemoveComponent(*child));
    CHECK(go->GetAllComponents().size() == 1);
    CHECK(root->GetAttachChildren().empty());

    // Parent transform notify must not touch freed child pointers.
    root->SetPosition(Vector3(1.0f, 2.0f, 3.0f));
    SceneManager::Get().SendAllEndOfFrameUpdates();
}

TEST_CASE("gameobject-hierarchy: attach without root fails [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    GameObjectHierarchyTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("go-hier-noroot");
    const std::shared_ptr<GameObject> parent = scene->CreateGameObject();
    const std::shared_ptr<GameObject> child = scene->CreateGameObject();
    parent->AddComponent<SceneComponent>();
    // child has no SceneComponent / Root

    CHECK_FALSE(child->AttachToParent(parent.get(), AttachmentTransformRules::KeepWorldTransform));
    CHECK(child->GetParent() == nullptr);
}
