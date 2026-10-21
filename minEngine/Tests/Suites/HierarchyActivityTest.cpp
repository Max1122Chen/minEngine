#include "HierarchyActivityTest.h"

#include "EngineTestFixture.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

#include <random>

#include "doctest.h"

namespace minEngine
{
    namespace
    {
        class HierarchyActivityTestScope
        {
        public:
            HierarchyActivityTestScope()
            {
                Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
                ObjectManager::Initialize();
                Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
                m_SceneManager.Initialize();
            }

            ~HierarchyActivityTestScope()
            {
                m_SceneManager.Shutdown();
                Testing::TestAccess<SceneManager>::SetInstance(nullptr);
                ObjectManager::Shutdown();
                Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
            }

        private:
            ObjectManager m_ObjectManager;
            SceneManager m_SceneManager;
        };

        /** The four observable values of one component around an ancestor switch. */
        struct ActivitySnapshot
        {
            bool activeSelf = true;
            bool active = true;
            bool visibleSelf = true;
            bool visible = true;
        };

        ActivitySnapshot Capture(const std::shared_ptr<SceneComponent>& component)
        {
            ActivitySnapshot snapshot;
            snapshot.activeSelf = component->IsActiveSelf();
            snapshot.active = component->IsActive();
            snapshot.visibleSelf = component->IsVisibleSelf();
            snapshot.visible = component->IsVisible();
            return snapshot;
        }

    }
}

TEST_CASE("hierarchy-activity: ancestor switch never writes authored flags [smoke]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    HierarchyActivityTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("hierarchy-activity-o1");
    REQUIRE(scene);

    const std::shared_ptr<GameObject> root = scene->CreateGameObject();
    REQUIRE(root);
    const std::shared_ptr<SceneComponent> rootComponent = root->AddComponent<SceneComponent>();
    REQUIRE(rootComponent);

    // This branch was authored OFF (inactive and hidden) before the ancestor is touched at all.
    const std::shared_ptr<GameObject> authoredOff = scene->CreateGameObject();
    REQUIRE(authoredOff);
    const std::shared_ptr<SceneComponent> authoredOffComponent =
        authoredOff->AddComponent<SceneComponent>();
    REQUIRE(authoredOffComponent);
    authoredOffComponent->SetActive(false);
    authoredOffComponent->SetVisible(false);
    REQUIRE(authoredOff->AttachToParent(root.get(), AttachmentTransformRules::KeepRelativeTransform));

    // This sibling stays authored ON and is the witness that only the effective state flips.
    const std::shared_ptr<GameObject> sibling = scene->CreateGameObject();
    REQUIRE(sibling);
    const std::shared_ptr<SceneComponent> siblingComponent = sibling->AddComponent<SceneComponent>();
    REQUIRE(siblingComponent);
    REQUIRE(sibling->AttachToParent(root.get(), AttachmentTransformRules::KeepRelativeTransform));

    const ActivitySnapshot authoredBefore = Capture(authoredOffComponent);
    const ActivitySnapshot siblingBefore = Capture(siblingComponent);
    CHECK_FALSE(authoredBefore.activeSelf);
    CHECK_FALSE(authoredBefore.visibleSelf);
    CHECK_FALSE(authoredBefore.active);
    CHECK_FALSE(authoredBefore.visible);
    CHECK(siblingBefore.activeSelf);
    CHECK(siblingBefore.visibleSelf);
    CHECK(siblingBefore.active);
    CHECK(siblingBefore.visible);

    // --- time 2: the ancestor goes off. Nothing may be written back. ---
    root->SetActive(false);
    root->SetVisible(false);
    CHECK_FALSE(root->IsActiveSelf());
    CHECK_FALSE(root->IsVisibleSelf());
    CHECK_FALSE(root->IsActive());
    CHECK_FALSE(root->IsVisible());
    CHECK_FALSE(rootComponent->IsActive());

    CHECK(authoredOffComponent->IsActiveSelf() == authoredBefore.activeSelf);
    CHECK(authoredOffComponent->IsVisibleSelf() == authoredBefore.visibleSelf);
    CHECK(siblingComponent->IsActiveSelf() == siblingBefore.activeSelf);
    CHECK(siblingComponent->IsVisibleSelf() == siblingBefore.visibleSelf);
    CHECK_FALSE(siblingComponent->IsActive());
    CHECK_FALSE(siblingComponent->IsVisible());
    CHECK_FALSE(siblingComponent->IsActiveAndVisible());

    // --- time 3: the ancestor comes back. Authored AND effective values restore exactly. ---
    root->SetActive(true);
    root->SetVisible(true);
    const ActivitySnapshot authoredAfter = Capture(authoredOffComponent);
    const ActivitySnapshot siblingAfter = Capture(siblingComponent);
    CHECK(authoredAfter.activeSelf == authoredBefore.activeSelf);
    CHECK(authoredAfter.active == authoredBefore.active);
    CHECK(authoredAfter.visibleSelf == authoredBefore.visibleSelf);
    CHECK(authoredAfter.visible == authoredBefore.visible);
    CHECK(siblingAfter.activeSelf == siblingBefore.activeSelf);
    CHECK(siblingAfter.active == siblingBefore.active);
    CHECK(siblingAfter.visibleSelf == siblingBefore.visibleSelf);
    CHECK(siblingAfter.visible == siblingBefore.visible);
}

TEST_CASE("hierarchy-activity: reconcile converges without self-excitation [smoke]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    HierarchyActivityTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("hierarchy-activity-o8");
    REQUIRE(scene);

    const std::shared_ptr<GameObject> root = scene->CreateGameObject();
    REQUIRE(root);
    const std::shared_ptr<SceneComponent> rootComponent = root->AddComponent<SceneComponent>();
    REQUIRE(rootComponent);

    const std::shared_ptr<GameObject> mid = scene->CreateGameObject();
    REQUIRE(mid);
    const std::shared_ptr<SceneComponent> midComponent = mid->AddComponent<SceneComponent>();
    REQUIRE(midComponent);
    REQUIRE(mid->AttachToParent(root.get(), AttachmentTransformRules::KeepRelativeTransform));

    const std::shared_ptr<GameObject> leaf = scene->CreateGameObject();
    REQUIRE(leaf);
    const std::shared_ptr<SceneComponent> leafComponent = leaf->AddComponent<SceneComponent>();
    REQUIRE(leafComponent);
    REQUIRE(leaf->AttachToParent(mid.get(), AttachmentTransformRules::KeepRelativeTransform));

    // Seeded random toggle sequence, used only to reach a rich state.
    std::mt19937 rng(0xC043F11u);
    std::uniform_int_distribution<int> target(0, 2);
    std::uniform_int_distribution<int> onOff(0, 1);
    for (int step = 0; step < 200; ++step)
    {
        GameObject* gameObjects[3] = { root.get(), mid.get(), leaf.get() };
        SceneComponent* components[3] = { rootComponent.get(), midComponent.get(), leafComponent.get() };
        const int which = target(rng);
        const bool value = onOff(rng) != 0;
        if (onOff(rng) != 0)
        {
            gameObjects[which]->SetActive(value);
        }
        else
        {
            components[which]->SetVisible(value);
        }
    }

    // Settle: root-and-below all authored on and visible.
    root->SetActive(true);
    root->SetVisible(true);
    mid->SetActive(true);
    leaf->SetActive(true);
    rootComponent->SetVisible(true);
    midComponent->SetVisible(true);
    leafComponent->SetVisible(true);

    const std::shared_ptr<SceneComponent> all[3] = { rootComponent, midComponent, leafComponent };
    uint32_t applyBefore[3] = { 0, 0, 0 };
    uint32_t removeBefore[3] = { 0, 0, 0 };
    for (int index = 0; index < 3; ++index)
    {
        REQUIRE(all[index]->IsActive());
        applyBefore[index] = all[index]->GetActivationApplyCount();
        removeBefore[index] = all[index]->GetActivationRemoveCount();
    }

    // A second (and third) reconcile in the settled state must register and unregister nothing.
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int index = 0; index < 3; ++index)
        {
            all[index]->ReconcileEffectiveActivation();
        }
    }
    for (int index = 0; index < 3; ++index)
    {
        CHECK(all[index]->GetActivationApplyCount() == applyBefore[index]);
        CHECK(all[index]->GetActivationRemoveCount() == removeBefore[index]);
        CHECK(all[index]->IsActive());
    }

    // Nobody changes state for 60 frames: the steady state must stay silent.
    for (int frame = 0; frame < 60; ++frame)
    {
        scene->Tick(1.0f / 60.0f);
    }
    for (int index = 0; index < 3; ++index)
    {
        CHECK(all[index]->GetActivationApplyCount() == applyBefore[index]);
        CHECK(all[index]->GetActivationRemoveCount() == removeBefore[index]);
        CHECK(all[index]->IsActive());
    }
}

TEST_CASE("hierarchy-activity: effective state follows the chain and survives re-parenting [smoke]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    HierarchyActivityTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("hierarchy-activity-chain");
    REQUIRE(scene);

    const std::shared_ptr<GameObject> root = scene->CreateGameObject();
    REQUIRE(root);
    REQUIRE(root->AddComponent<SceneComponent>());

    const std::shared_ptr<GameObject> mid = scene->CreateGameObject();
    REQUIRE(mid);
    const std::shared_ptr<SceneComponent> midComponent = mid->AddComponent<SceneComponent>();
    REQUIRE(midComponent);
    REQUIRE(mid->AttachToParent(root.get(), AttachmentTransformRules::KeepRelativeTransform));

    const std::shared_ptr<GameObject> leaf = scene->CreateGameObject();
    REQUIRE(leaf);
    const std::shared_ptr<SceneComponent> leafComponent = leaf->AddComponent<SceneComponent>();
    REQUIRE(leafComponent);
    REQUIRE(leaf->AttachToParent(mid.get(), AttachmentTransformRules::KeepRelativeTransform));

    // A grandparent switch reaches the grandchild through the cached effective state.
    root->SetActive(false);
    CHECK_FALSE(leaf->IsActive());
    CHECK_FALSE(leafComponent->IsActive());
    CHECK(leaf->IsActiveSelf());
    CHECK(leafComponent->IsActiveSelf());
    root->SetActive(true);
    CHECK(leaf->IsActive());
    CHECK(leafComponent->IsActive());

    // A middle switch reaches the leaf and leaves the authored flags alone.
    mid->SetActive(false);
    CHECK_FALSE(leaf->IsActive());
    CHECK_FALSE(leafComponent->IsActive());
    CHECK(leafComponent->IsActiveSelf());
    mid->SetActive(true);
    CHECK(leaf->IsActive());
    CHECK(leafComponent->IsActive());

    // Detaching under an inactive ancestor must re-evaluate on the way out.
    mid->SetActive(false);
    CHECK_FALSE(leafComponent->IsActive());
    leaf->DetachFromParent(AttachmentTransformRules::KeepWorldTransform);
    CHECK(leaf->GetParent() == nullptr);
    CHECK(leaf->IsActive());
    CHECK(leafComponent->IsActive());
    CHECK(leafComponent->IsActiveSelf());
    mid->SetActive(true);

    // Re-attach so the visibility axis below is exercised through the chain again (trigger 2).
    REQUIRE(leaf->AttachToParent(mid.get(), AttachmentTransformRules::KeepRelativeTransform));

    // IsActiveAndVisible is the draw/hit gate: hidden by an ancestor keeps the authored flag.
    root->SetVisible(false);
    CHECK(leafComponent->IsVisibleSelf());
    CHECK_FALSE(leafComponent->IsVisible());
    CHECK(leafComponent->IsActive());
    CHECK_FALSE(leafComponent->IsActiveAndVisible());
    CHECK(root->IsVisibleInHierarchy() == false);
    root->SetVisible(true);
    CHECK(leafComponent->IsActiveAndVisible());
    CHECK(leafComponent->IsVisibleInHierarchy());
    CHECK(leafComponent->IsActiveInHierarchy());

    // Authoring the component itself hidden is symmetric and never touches activation.
    leafComponent->SetVisible(false);
    CHECK(leafComponent->IsActive());
    CHECK_FALSE(leafComponent->IsVisibleSelf());
    CHECK_FALSE(leafComponent->IsActiveAndVisible());
    leafComponent->SetVisible(true);
    CHECK(leafComponent->IsActiveAndVisible());
}