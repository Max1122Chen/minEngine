#include "PrefabOverridesTest.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Function/Framework/Prefab/PrefabEditValidator.h"
#include "Runtime/Function/Framework/Prefab/PrefabOverrideUtility.h"
#include "Runtime/Function/Framework/Prefab/PrefabPropertyPath.h"
#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

#include "doctest.h"

namespace minEngine
{
    class PrefabOverridesTestScope
    {
    public:
        PrefabOverridesTestScope()
        {
            Reflection::ReflectionSystem& reflection = Reflection::ReflectionSystem::Get();
            if (!reflection.IsReady())
            {
                REQUIRE(reflection.FinalizeReflection());
                reflection.ClearErrors();
            }

            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();
            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~PrefabOverridesTestScope()
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

    TEST_CASE("prefab-overrides record and revert property [smoke]")
    {
        PrefabOverridesTestScope scope;

        std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("prefab-ov-record");
        REQUIRE(static_cast<bool>(scene));
        scene->SetSceneType(ESceneType::Editor);

        std::shared_ptr<GameObject> root = scene->CreateGameObject();
        root->Rename("TemplateRoot");
        root->AddComponent<SceneComponent>();

        std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*root);
        REQUIRE(static_cast<bool>(prefab));

        PrefabInstantiateParams params;
        params.bRegisterPrefabInstance = true;
        std::shared_ptr<GameObject> instance =
            PrefabUtility::Instantiate(*prefab, *scene, params);
        REQUIRE(static_cast<bool>(instance));

        PrefabInstanceRecord* record = PrefabUtility::FindInstanceRecord(*scene, instance->GetGuid());
        REQUIRE(record != nullptr);

        instance->Rename("OverriddenName");
        REQUIRE(PrefabOverrideUtility::TryRecordPropertyOverride(*scene, *instance, "m_Name"));
        CHECK(PrefabOverrideUtility::HasOverride(
            *record,
            PrefabPropertyPath::FindTemplateGuidForInstance(*record, instance->GetGuid()),
            "m_Name"));

        REQUIRE(PrefabOverrideUtility::RevertProperty(*scene, *instance, "m_Name"));
        CHECK(instance->GetName() == prefab->GetRootGameObject()->GetName());
        CHECK_FALSE(PrefabOverrideUtility::HasOverride(
            *record,
            PrefabPropertyPath::FindTemplateGuidForInstance(*record, instance->GetGuid()),
            "m_Name"));
    }

    TEST_CASE("prefab-overrides propagate skips overridden and root transform [smoke]")
    {
        PrefabOverridesTestScope scope;

        std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("prefab-ov-prop");
        REQUIRE(static_cast<bool>(scene));
        scene->SetSceneType(ESceneType::Editor);

        std::shared_ptr<GameObject> root = scene->CreateGameObject();
        root->Rename("DefaultName");
        auto rootComponent = root->AddComponent<SceneComponent>();
        rootComponent->SetPosition(Vector3(1.0f, 0.0f, 0.0f));

        std::shared_ptr<GameObject> child = scene->CreateGameObject();
        child->Rename("Child");
        auto childComponent = child->AddComponent<SceneComponent>();
        childComponent->SetPosition(Vector3(0.0f, 2.0f, 0.0f));
        REQUIRE(child->AttachToParent(root.get(), AttachmentTransformRules::KeepRelativeTransform));

        std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*root);
        REQUIRE(static_cast<bool>(prefab));

        // Keep prefab registered in ObjectManager under its Guid for ResolvePrefabAsset.
        PrefabInstantiateParams params;
        std::shared_ptr<GameObject> instance = PrefabUtility::Instantiate(*prefab, *scene, params);
        REQUIRE(static_cast<bool>(instance));

        PrefabInstanceRecord* record = PrefabUtility::FindInstanceRecord(*scene, instance->GetGuid());
        REQUIRE(record != nullptr);
        record->PrefabAssetGuid = prefab->GetGuid();

        instance->Rename("InstanceOverride");
        REQUIRE(PrefabOverrideUtility::TryRecordPropertyOverride(*scene, *instance, "m_Name"));

        // Change template defaults.
        // Note: Prefab template trees keep parent Guid links but do not rebuild m_Children;
        // find the non-root template object via GetTemplateObjects.
        prefab->GetRootGameObject()->Rename("PropagatedName");
        GameObject* templateRoot = prefab->GetRootGameObject();
        GameObject* templateChild = nullptr;
        for (const std::shared_ptr<GameObject>& templateObject : prefab->GetTemplateObjects())
        {
            if (templateObject && templateObject.get() != templateRoot)
            {
                templateChild = templateObject.get();
                break;
            }
        }
        REQUIRE(templateChild != nullptr);
        REQUIRE(templateChild->GetRootComponent() != nullptr);
        templateChild->GetRootComponent()->SetPosition(Vector3(0.0f, 9.0f, 0.0f));
        templateRoot->GetRootComponent()->SetPosition(Vector3(5.0f, 5.0f, 5.0f));

        PrefabOverrideUtility::PropagateDefaultsToScene(*prefab, *scene);

        CHECK(instance->GetName() == "InstanceOverride");
        // Root transform must not pick up template (5,5,5).
        CHECK(instance->GetRootComponent()->GetPosition().x != doctest::Approx(5.0f));

        REQUIRE(instance->GetChildren().size() == 1);
        GameObject* instanceChild = instance->GetChildren()[0];
        REQUIRE(instanceChild->GetRootComponent() != nullptr);
        CHECK(instanceChild->GetRootComponent()->GetPosition().y == doctest::Approx(9.0f));
    }

    TEST_CASE("prefab-overrides validate rejects delete root [smoke]")
    {
        PrefabOverridesTestScope scope;

        std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("prefab-ov-val");
        REQUIRE(static_cast<bool>(scene));
        scene->SetSceneType(ESceneType::Editor);

        std::shared_ptr<GameObject> root = scene->CreateGameObject();
        root->AddComponent<SceneComponent>();
        std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*root);
        REQUIRE(static_cast<bool>(prefab));

        PrefabInstanceRecord* record = PrefabUtility::FindInstanceRecord(*scene, root->GetGuid());
        REQUIRE(record != nullptr);

        PrefabEditOp op;
        op.Kind = EPrefabEditOpKind::DeleteGameObject;
        op.TargetInstanceGuid = record->RootInstanceGuid;
        PrefabEditValidationResult validation =
            PrefabOverrideUtility::ValidateEdit(*scene, *record, op);
        CHECK_FALSE(validation.bAllowed);
    }
}
