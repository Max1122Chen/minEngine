#include "PrefabTest.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneDuplicator.h"
#include "Runtime/Function/Framework/Scene/SceneCloneContext.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

#include <doctest.h>
#include <filesystem>

namespace minEngine
{
    class PrefabTestScope
    {
    public:
        PrefabTestScope()
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

        ~PrefabTestScope()
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

    TEST_CASE("prefab create instantiate hierarchy [smoke]")
    {
        PrefabTestScope scope;

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("prefab-test");
        REQUIRE(scene != nullptr);
        scene->SetSceneType(ESceneType::Editor);

        const std::shared_ptr<GameObject> parent = scene->CreateGameObject();
        parent->Rename("Parent");
        parent->AddComponent<SceneComponent>();

        const std::shared_ptr<GameObject> child = scene->CreateGameObject();
        child->Rename("Child");
        child->AddComponent<SceneComponent>();
        REQUIRE(child->AttachToParent(parent.get(), AttachmentTransformRules::KeepRelativeTransform));

        const GUID sourceParentGuid = parent->GetGuid();
        const GUID sourceChildGuid = child->GetGuid();

        PrefabCreateReport report;
        const std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*parent, &report);
        REQUIRE(prefab != nullptr);
        CHECK(prefab->ValidateSingleRoot());
        CHECK(prefab->GetTemplateObjects().size() == 2);
        CHECK(PrefabUtility::FindInstanceRecord(*scene, sourceParentGuid) != nullptr);

        const std::shared_ptr<Scene> targetScene = SceneManager::Get().CreateNewScene("prefab-target");
        REQUIRE(targetScene != nullptr);
        targetScene->SetSceneType(ESceneType::Editor);

        PrefabInstantiateParams params;
        params.WorldTransform.Position = Vector3(10.0f, 0.0f, 0.0f);
        params.bRegisterPrefabInstance = true;

        std::string error;
        const std::shared_ptr<GameObject> instanceRoot =
            PrefabUtility::Instantiate(*prefab, *targetScene, params, &error);
        REQUIRE(instanceRoot != nullptr);
        CHECK(error.empty());
        CHECK(instanceRoot->GetGuid() != sourceParentGuid);
        CHECK(instanceRoot->GetGuid() != prefab->GetRootGuid());
        CHECK(instanceRoot->GetChildren().size() == 1);
        CHECK(PrefabUtility::FindInstanceRecord(*targetScene, instanceRoot->GetGuid()) != nullptr);
        CHECK(targetScene->GetPrefabInstances().front().Overrides.empty());
    }

    TEST_CASE("prefab disk roundtrip [smoke]")
    {
        PrefabTestScope scope;

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("prefab-disk");
        REQUIRE(scene != nullptr);
        scene->SetSceneType(ESceneType::Editor);

        const std::shared_ptr<GameObject> root = scene->CreateGameObject();
        root->Rename("DiskRoot");
        root->AddComponent<SceneComponent>()->SetPosition(Vector3(1.0f, 2.0f, 3.0f));

        const std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*root);
        REQUIRE(prefab != nullptr);

        const std::filesystem::path tempPath =
            std::filesystem::temp_directory_path() / "minEngine_prefab_roundtrip.meprefab";

        Serialization::JsonWriterArchive writer;
        const Serialization::SerializeResult writeResult = Serialization::Serializer::ToFile(
            tempPath.string(),
            prefab.get(),
            writer,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = false,
                .skipUnknownField = false});
        REQUIRE(writeResult.ok);

        std::shared_ptr<Prefab> loaded = NewObject<Prefab>("Loaded");
        Serialization::JsonReaderArchive reader;
        const Serialization::SerializeResult readResult = Serialization::Serializer::FromFile(
            tempPath.string(),
            loaded.get(),
            reader,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = false,
                .skipUnknownField = true});
        REQUIRE(readResult.ok);
        CHECK(loaded->ValidateSingleRoot());
        CHECK(loaded->GetTemplateObjects().size() == prefab->GetTemplateObjects().size());

        GameObject* loadedRoot = loaded->GetRootGameObject();
        REQUIRE(loadedRoot != nullptr);
        REQUIRE(loadedRoot->GetRootComponent() != nullptr);
        CHECK(loadedRoot->GetRootComponent()->GetPosition().x == doctest::Approx(1.0f));

        std::error_code removeError;
        std::filesystem::remove(tempPath, removeError);
    }

    TEST_CASE("prefab pie clears instance records [smoke]")
    {
        PrefabTestScope scope;

        const std::shared_ptr<Scene> editorScene = SceneManager::Get().CreateNewScene("prefab-pie");
        REQUIRE(editorScene != nullptr);
        editorScene->SetSceneType(ESceneType::Editor);

        const std::shared_ptr<GameObject> root = editorScene->CreateGameObject();
        root->AddComponent<SceneComponent>();
        const std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*root);
        REQUIRE(prefab != nullptr);
        REQUIRE(!editorScene->GetPrefabInstances().empty());

        SceneCloneContext cloneContext;
        const std::shared_ptr<Scene> pieScene = SceneDuplicator::DuplicateForPIE(*editorScene, cloneContext);
        REQUIRE(pieScene != nullptr);
        CHECK(pieScene->GetPrefabInstances().empty());
        CHECK(!pieScene->GetAllGameObjects().empty());
    }
}
