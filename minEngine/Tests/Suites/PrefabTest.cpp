#include "PrefabTest.h"

#include "Access/AssetManagerTestAccess.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "AssetManager.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectCloneContext.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Paths/PathRegistry.h"
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
        REQUIRE(static_cast<bool>(scene));
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
        REQUIRE(static_cast<bool>(prefab));
        CHECK(prefab->ValidateSingleRoot());
        CHECK(prefab->GetTemplateObjects().size() == 2);
        CHECK(PrefabUtility::FindInstanceRecord(*scene, sourceParentGuid) != nullptr);

        const std::shared_ptr<Scene> targetScene = SceneManager::Get().CreateNewScene("prefab-target");
        REQUIRE(static_cast<bool>(targetScene));
        targetScene->SetSceneType(ESceneType::Editor);

        PrefabInstantiateParams params;
        params.WorldTransform.Position = Vector3(10.0f, 0.0f, 0.0f);
        params.bApplyWorldTransform = true;
        params.bRegisterPrefabInstance = true;

        std::string error;
        const std::shared_ptr<GameObject> instanceRoot =
            PrefabUtility::Instantiate(*prefab, *targetScene, params, &error);
        REQUIRE(static_cast<bool>(instanceRoot));
        CHECK(error.empty());
        CHECK(instanceRoot->GetGuid() != sourceParentGuid);
        CHECK(instanceRoot->GetGuid() != prefab->GetRootGuid());
        CHECK(instanceRoot->GetChildren().size() == 1);
        CHECK(PrefabUtility::FindInstanceRecord(*targetScene, instanceRoot->GetGuid()) != nullptr);
        CHECK(targetScene->GetPrefabInstances().front().Overrides.empty());
    }

    TEST_CASE("prefab scene instantiate convenience [smoke]")
    {
        PrefabTestScope scope;

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("prefab-scene-instantiate");
        REQUIRE(static_cast<bool>(scene));
        scene->SetSceneType(ESceneType::Editor);

        const std::shared_ptr<GameObject> root = scene->CreateGameObject();
        root->Rename("ConvRoot");
        root->AddComponent<SceneComponent>();

        const std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*root);
        REQUIRE(static_cast<bool>(prefab));

        const std::shared_ptr<Scene> targetScene = SceneManager::Get().CreateNewScene("prefab-scene-target");
        REQUIRE(static_cast<bool>(targetScene));
        targetScene->SetSceneType(ESceneType::Editor);

        PrefabInstantiateParams params;
        params.bRegisterPrefabInstance = true;
        std::string error;
        const std::shared_ptr<GameObject> instanceRoot = targetScene->Instantiate(*prefab, params, &error);
        REQUIRE(static_cast<bool>(instanceRoot));
        CHECK(error.empty());
        CHECK(PrefabUtility::FindInstanceRecord(*targetScene, instanceRoot->GetGuid()) != nullptr);
        CHECK(instanceRoot->GetOuter() == targetScene.get());
    }

    TEST_CASE("prefab disk roundtrip [smoke]")
    {
        PrefabTestScope scope;

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("prefab-disk");
        REQUIRE(static_cast<bool>(scene));
        scene->SetSceneType(ESceneType::Editor);

        const std::shared_ptr<GameObject> root = scene->CreateGameObject();
        root->Rename("DiskRoot");
        root->AddComponent<SceneComponent>()->SetPosition(Vector3(1.0f, 2.0f, 3.0f));

        const std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*root);
        REQUIRE(static_cast<bool>(prefab));

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
        REQUIRE(static_cast<bool>(loadedRoot));
        REQUIRE(loadedRoot->GetRootComponent() != nullptr);
        CHECK(loadedRoot->GetRootComponent()->GetPosition().x == doctest::Approx(1.0f));

        std::error_code removeError;
        std::filesystem::remove(tempPath, removeError);
    }

    TEST_CASE("prefab pie clears instance records [smoke]")
    {
        PrefabTestScope scope;

        const std::shared_ptr<Scene> editorScene = SceneManager::Get().CreateNewScene("prefab-pie");
        REQUIRE(static_cast<bool>(editorScene));
        editorScene->SetSceneType(ESceneType::Editor);

        const std::shared_ptr<GameObject> root = editorScene->CreateGameObject();
        root->AddComponent<SceneComponent>();
        const std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*root);
        REQUIRE(static_cast<bool>(prefab));
        REQUIRE(!editorScene->GetPrefabInstances().empty());

        SceneCloneContext cloneContext;
        const std::shared_ptr<Scene> pieScene = SceneDuplicator::DuplicateForPIE(*editorScene, cloneContext);
        REQUIRE(static_cast<bool>(pieScene));
        CHECK(pieScene->GetPrefabInstances().empty());
        CHECK(!pieScene->GetAllGameObjects().empty());
    }

    TEST_CASE("prefab stage writeback preserves template guids [smoke]")
    {
        PrefabTestScope scope;

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("prefab-writeback");
        REQUIRE(static_cast<bool>(scene));
        scene->SetSceneType(ESceneType::Editor);

        const std::shared_ptr<GameObject> root = scene->CreateGameObject();
        root->Rename("WBRoot");
        root->AddComponent<SceneComponent>()->SetPosition(Vector3(1.0f, 0.0f, 0.0f));

        const std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*root);
        REQUIRE(static_cast<bool>(prefab));
        const GUID templateRootGuid = prefab->GetRootGuid();
        REQUIRE_FALSE(templateRootGuid.IsZero());

        std::shared_ptr<Scene> stage = NewObject<Scene>("Stage");
        stage->SetSceneType(ESceneType::Editor);
        stage->EnsureRenderScene();

        PrefabInstantiateParams params;
        params.bRegisterPrefabInstance = false;
        ObjectCloneContext editMap;
        std::shared_ptr<GameObject> stageRoot =
            PrefabUtility::Instantiate(*prefab, *stage, params, &editMap, nullptr);
        REQUIRE(static_cast<bool>(stageRoot));
        stageRoot->Rename("EditedRoot");
        REQUIRE(stageRoot->GetRootComponent() != nullptr);
        stageRoot->GetRootComponent()->SetPosition(Vector3(9.0f, 8.0f, 7.0f));

        REQUIRE(PrefabUtility::WriteStageTreeToPrefab(*stage, *prefab, editMap, nullptr));
        CHECK(prefab->GetRootGuid() == templateRootGuid);
        GameObject* writtenRoot = prefab->GetRootGameObject();
        REQUIRE(writtenRoot != nullptr);
        CHECK(writtenRoot->GetGuid() == templateRootGuid);
        CHECK(writtenRoot->GetName() == "EditedRoot");
        REQUIRE(writtenRoot->GetRootComponent() != nullptr);
        CHECK(writtenRoot->GetRootComponent()->GetPosition().x == doctest::Approx(9.0f));
    }

    TEST_CASE("prefab create save keeps asset guid with instance record [smoke]")
    {
        PrefabTestScope scope;

        AssetManager assetManager;
        Testing::TestAccess<AssetManager>::SetInstance(&assetManager);
        assetManager.Initialize();

        const std::filesystem::path tempRoot =
            std::filesystem::temp_directory_path() / "minEngine_PrefabCreateGuidTest";
        std::error_code removeError;
        std::filesystem::remove_all(tempRoot, removeError);
        const std::filesystem::path projectRoot = tempRoot / "Project";
        const std::filesystem::path contentRoot = projectRoot / "Assets" / "Prefabs";
        std::filesystem::create_directories(contentRoot);
        PathRegistry::Get().SetProjectRoots(projectRoot);

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("prefab-guid");
        REQUIRE(static_cast<bool>(scene));
        scene->SetSceneType(ESceneType::Editor);

        const std::shared_ptr<GameObject> root = scene->CreateGameObject();
        root->Rename("GuidRoot");
        root->AddComponent<SceneComponent>();

        const GUID rootGuid = root->GetGuid();
        const std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*root);
        REQUIRE(static_cast<bool>(prefab));

        const PrefabInstanceRecord* recordBeforeSave =
            PrefabUtility::FindInstanceRecord(*scene, rootGuid);
        REQUIRE(recordBeforeSave != nullptr);
        const GUID recordedGuid = recordBeforeSave->PrefabAssetGuid;
        CHECK(recordedGuid == prefab->GetGuid());

        const std::string assetPath = "Assets/Prefabs/GuidCube.meprefab";
        std::string saveError;
        REQUIRE(PrefabUtility::SavePrefabAsset(*prefab, assetPath, &saveError));
        CHECK(saveError.empty());

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(assetPath);
        REQUIRE(meta != nullptr);
        CHECK(meta->Guid == prefab->GetGuid());
        CHECK(meta->Guid == recordedGuid);

        const PrefabInstanceRecord* recordAfterSave =
            PrefabUtility::FindInstanceRecord(*scene, rootGuid);
        REQUIRE(recordAfterSave != nullptr);
        CHECK(recordAfterSave->PrefabAssetGuid == meta->Guid);

        assetManager.Shutdown();
        Testing::TestAccess<AssetManager>::SetInstance(nullptr);
        PathRegistry::Get().ClearProjectRoots();
        std::filesystem::remove_all(tempRoot, removeError);
    }

    TEST_CASE("prefab instantiate preserves template transform unless applied [smoke]")
    {
        PrefabTestScope scope;

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("prefab-xform-src");
        REQUIRE(static_cast<bool>(scene));
        scene->SetSceneType(ESceneType::Editor);

        const std::shared_ptr<GameObject> root = scene->CreateGameObject();
        root->Rename("XformRoot");
        auto rootComponent = root->AddComponent<SceneComponent>();
        REQUIRE(rootComponent != nullptr);
        rootComponent->SetPosition(Vector3(3.0f, 4.0f, 5.0f));
        rootComponent->SetScale(Vector3(2.0f, 2.0f, 2.0f));

        const std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*root);
        REQUIRE(static_cast<bool>(prefab));
        GameObject* templateRoot = prefab->GetRootGameObject();
        REQUIRE(templateRoot != nullptr);
        REQUIRE(templateRoot->GetRootComponent() != nullptr);
        CHECK(templateRoot->GetRootComponent()->GetPosition().x == doctest::Approx(3.0f));
        CHECK(templateRoot->GetRootComponent()->GetScale().x == doctest::Approx(2.0f));

        const std::shared_ptr<Scene> stage = NewObject<Scene>("StageXform");
        stage->SetSceneType(ESceneType::Editor);
        stage->EnsureRenderScene();

        PrefabInstantiateParams stageParams;
        stageParams.bRegisterPrefabInstance = false;
        ObjectCloneContext editMap;
        const std::shared_ptr<GameObject> stageRoot =
            PrefabUtility::Instantiate(*prefab, *stage, stageParams, &editMap, nullptr);
        REQUIRE(static_cast<bool>(stageRoot));
        REQUIRE(stageRoot->GetRootComponent() != nullptr);
        CHECK(stageRoot->GetRootComponent()->GetPosition().x == doctest::Approx(3.0f));
        CHECK(stageRoot->GetRootComponent()->GetScale().x == doctest::Approx(2.0f));
        CHECK(editMap.SourceToClonedGuid.find(prefab->GetRootGuid()) != editMap.SourceToClonedGuid.end());

        const std::shared_ptr<Scene> level = SceneManager::Get().CreateNewScene("prefab-xform-level");
        REQUIRE(static_cast<bool>(level));
        level->SetSceneType(ESceneType::Editor);

        PrefabInstantiateParams levelParams;
        levelParams.bRegisterPrefabInstance = true;
        levelParams.bApplyWorldTransform = true;
        levelParams.WorldTransform.Position = Vector3(10.0f, 0.0f, 0.0f);
        const std::shared_ptr<GameObject> levelRoot =
            PrefabUtility::Instantiate(*prefab, *level, levelParams, static_cast<std::string*>(nullptr));
        REQUIRE(static_cast<bool>(levelRoot));
        REQUIRE(levelRoot->GetRootComponent() != nullptr);
        CHECK(levelRoot->GetRootComponent()->GetPosition().x == doctest::Approx(10.0f));
    }

    TEST_CASE("prefab create bakes source world scale [smoke]")
    {
        PrefabTestScope scope;

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("prefab-world-scale");
        REQUIRE(static_cast<bool>(scene));
        scene->SetSceneType(ESceneType::Editor);

        const std::shared_ptr<GameObject> parent = scene->CreateGameObject();
        parent->Rename("ScaledParent");
        parent->AddComponent<SceneComponent>()->SetScale(Vector3(2.0f, 2.0f, 2.0f));

        const std::shared_ptr<GameObject> child = scene->CreateGameObject();
        child->Rename("ChildRoot");
        child->AddComponent<SceneComponent>()->SetScale(Vector3(3.0f, 3.0f, 3.0f));
        REQUIRE(child->AttachToParent(parent.get(), AttachmentTransformRules::KeepRelativeTransform));

        const Transform sourceWorldBefore = child->GetWorldTransform();
        CHECK(sourceWorldBefore.Scale.x == doctest::Approx(6.0f));

        const std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*child);
        REQUIRE(static_cast<bool>(prefab));

        const Transform sourceWorldAfter = child->GetWorldTransform();
        CHECK(sourceWorldAfter.Scale.x == doctest::Approx(sourceWorldBefore.Scale.x));
        CHECK(sourceWorldAfter.Position.x == doctest::Approx(sourceWorldBefore.Position.x));

        GameObject* templateRoot = prefab->GetRootGameObject();
        REQUIRE(templateRoot != nullptr);
        CHECK(templateRoot->GetParent() == nullptr);
        REQUIRE(templateRoot->GetRootComponent() != nullptr);
        CHECK(templateRoot->GetRootComponent()->GetScale().x == doctest::Approx(6.0f));
    }

    TEST_CASE("prefab stage writeback twice keeps edit map root [smoke]")
    {
        PrefabTestScope scope;

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("prefab-writeback-2");
        REQUIRE(static_cast<bool>(scene));
        scene->SetSceneType(ESceneType::Editor);

        const std::shared_ptr<GameObject> root = scene->CreateGameObject();
        root->Rename("WB2Root");
        root->AddComponent<SceneComponent>()->SetPosition(Vector3(1.0f, 0.0f, 0.0f));

        const std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*root);
        REQUIRE(static_cast<bool>(prefab));
        const GUID templateRootGuid = prefab->GetRootGuid();

        std::shared_ptr<Scene> stage = NewObject<Scene>("Stage2");
        stage->SetSceneType(ESceneType::Editor);
        stage->EnsureRenderScene();

        PrefabInstantiateParams params;
        params.bRegisterPrefabInstance = false;
        ObjectCloneContext editMap;
        std::shared_ptr<GameObject> stageRoot =
            PrefabUtility::Instantiate(*prefab, *stage, params, &editMap, nullptr);
        REQUIRE(static_cast<bool>(stageRoot));
        REQUIRE(editMap.SourceToClonedGuid.find(templateRootGuid) != editMap.SourceToClonedGuid.end());

        stageRoot->GetRootComponent()->SetPosition(Vector3(2.0f, 0.0f, 0.0f));
        std::string writeError;
        REQUIRE(PrefabUtility::WriteStageTreeToPrefab(*stage, *prefab, editMap, &writeError));
        CHECK(writeError.empty());
        CHECK(prefab->GetRootGuid() == templateRootGuid);
        CHECK(editMap.SourceToClonedGuid.find(prefab->GetRootGuid()) != editMap.SourceToClonedGuid.end());

        // Simulate the old bug: ObjectManager lost the stage root Guid slot.
        if (ObjectManager::HasInstance())
        {
            ObjectManager::Get().UnregisterObject(stageRoot->GetGuid());
        }

        stageRoot->GetRootComponent()->SetPosition(Vector3(3.0f, 0.0f, 0.0f));
        writeError.clear();
        REQUIRE(PrefabUtility::WriteStageTreeToPrefab(*stage, *prefab, editMap, &writeError));
        CHECK(writeError.empty());
        CHECK(prefab->GetRootGuid() == templateRootGuid);
        CHECK(editMap.SourceToClonedGuid.find(prefab->GetRootGuid()) != editMap.SourceToClonedGuid.end());
        GameObject* writtenRoot = prefab->GetRootGameObject();
        REQUIRE(writtenRoot != nullptr);
        REQUIRE(writtenRoot->GetRootComponent() != nullptr);
        CHECK(writtenRoot->GetRootComponent()->GetPosition().x == doctest::Approx(3.0f));
    }

    TEST_CASE("prefab unpack and delete gate [smoke]")
    {
        PrefabTestScope scope;

        AssetManager assetManager;
        Testing::TestAccess<AssetManager>::SetInstance(&assetManager);
        assetManager.Initialize();

        const std::filesystem::path tempRoot =
            std::filesystem::temp_directory_path() / "minEngine_PrefabUnpackDeleteTest";
        std::error_code removeError;
        std::filesystem::remove_all(tempRoot, removeError);
        const std::filesystem::path projectRoot = tempRoot / "Project";
        const std::filesystem::path contentRoot = projectRoot / "Assets" / "Prefabs";
        std::filesystem::create_directories(contentRoot);
        PathRegistry::Get().SetProjectRoots(projectRoot);

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("prefab-unpack-delete");
        REQUIRE(static_cast<bool>(scene));
        scene->SetSceneType(ESceneType::Editor);

        const std::shared_ptr<GameObject> root = scene->CreateGameObject();
        root->Rename("UnpackRoot");
        root->AddComponent<SceneComponent>();
        const GUID rootGuid = root->GetGuid();

        const std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*root);
        REQUIRE(static_cast<bool>(prefab));
        REQUIRE(PrefabUtility::FindInstanceRecord(*scene, rootGuid) != nullptr);

        const std::string assetPath = "Assets/Prefabs/UnpackCube.meprefab";
        std::string saveError;
        REQUIRE(PrefabUtility::SavePrefabAsset(*prefab, assetPath, &saveError));
        CHECK(saveError.empty());

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(assetPath);
        REQUIRE(meta != nullptr);

        std::vector<PrefabInstanceRef> refs =
            PrefabUtility::FindInstanceRefsInOpenEditorScenes(meta->Guid);
        REQUIRE(refs.size() == 1);
        CHECK(refs.front().RootInstanceGuid == rootGuid);

        std::string deleteError;
        REQUIRE_FALSE(AssetManager::Get().DeleteAsset(assetPath, deleteError, false));
        CHECK(deleteError.find("Cannot delete Prefab") != std::string::npos);
        CHECK(AssetManager::Get().FindAssetMetaByPath(assetPath) != nullptr);
        CHECK(PrefabUtility::FindInstanceRecord(*scene, rootGuid) != nullptr);

        size_t unpacked = 0;
        REQUIRE(PrefabUtility::UnpackAllInstancesOfPrefab(*scene, meta->Guid, &unpacked, nullptr));
        CHECK(unpacked == 1);
        CHECK(PrefabUtility::FindInstanceRecord(*scene, rootGuid) == nullptr);
        CHECK(ObjectManager::Get().FindObject(rootGuid) != nullptr);

        deleteError.clear();
        REQUIRE(AssetManager::Get().DeleteAsset(assetPath, deleteError, false));
        CHECK(deleteError.empty());
        CHECK(AssetManager::Get().FindAssetMetaByPath(assetPath) == nullptr);

        // Recreate for unpack-on-delete path.
        const std::shared_ptr<GameObject> root2 = scene->CreateGameObject();
        root2->Rename("UnpackRoot2");
        root2->AddComponent<SceneComponent>();
        const GUID root2Guid = root2->GetGuid();
        const std::shared_ptr<Prefab> prefab2 = PrefabUtility::CreatePrefabFromGameObject(*root2);
        REQUIRE(static_cast<bool>(prefab2));
        const std::string assetPath2 = "Assets/Prefabs/UnpackCube2.meprefab";
        REQUIRE(PrefabUtility::SavePrefabAsset(*prefab2, assetPath2, &saveError));
        REQUIRE(PrefabUtility::FindInstanceRecord(*scene, root2Guid) != nullptr);

        deleteError.clear();
        REQUIRE(AssetManager::Get().DeleteAsset(assetPath2, deleteError, true));
        CHECK(deleteError.empty());
        CHECK(PrefabUtility::FindInstanceRecord(*scene, root2Guid) == nullptr);
        CHECK(ObjectManager::Get().FindObject(root2Guid) != nullptr);
        CHECK(AssetManager::Get().FindAssetMetaByPath(assetPath2) == nullptr);

        assetManager.Shutdown();
        Testing::TestAccess<AssetManager>::SetInstance(nullptr);
        PathRegistry::Get().ClearProjectRoots();
        std::filesystem::remove_all(tempRoot, removeError);
    }

    TEST_CASE("prefab stage temp light excluded from save [smoke]")
    {
        PrefabTestScope scope;

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("prefab-temp-light-src");
        REQUIRE(static_cast<bool>(scene));
        scene->SetSceneType(ESceneType::Editor);

        const std::shared_ptr<GameObject> root = scene->CreateGameObject();
        root->Rename("LitRoot");
        root->AddComponent<SceneComponent>();

        const std::shared_ptr<Prefab> prefab = PrefabUtility::CreatePrefabFromGameObject(*root);
        REQUIRE(static_cast<bool>(prefab));
        const GUID templateRootGuid = prefab->GetRootGuid();
        const size_t templateCountBefore = prefab->GetTemplateObjects().size();

        std::shared_ptr<Scene> stage = NewObject<Scene>("StageTempLight");
        stage->SetSceneType(ESceneType::Editor);
        stage->EnsureRenderScene();

        PrefabInstantiateParams params;
        params.bRegisterPrefabInstance = false;
        ObjectCloneContext editMap;
        std::shared_ptr<GameObject> stageRoot =
            PrefabUtility::Instantiate(*prefab, *stage, params, &editMap, nullptr);
        REQUIRE(static_cast<bool>(stageRoot));

        std::shared_ptr<GameObject> tempLight = stage->CreateGameObject();
        REQUIRE(static_cast<bool>(tempLight));
        tempLight->Rename(std::string(PrefabUtility::kEditorTempStageObjectNamePrefix) + "DirectionalLight");
        tempLight->AddComponent<SceneComponent>();

        size_t topLevel = 0;
        size_t tempTopLevel = 0;
        for (const std::shared_ptr<GameObject>& go : stage->GetAllGameObjects())
        {
            if (!go || go->GetParent() != nullptr)
            {
                continue;
            }
            ++topLevel;
            if (PrefabUtility::IsEditorTempStageObject(*go))
            {
                ++tempTopLevel;
            }
        }
        CHECK(topLevel == 2);
        CHECK(tempTopLevel == 1);

        std::string writeError;
        REQUIRE(PrefabUtility::WriteStageTreeToPrefab(*stage, *prefab, editMap, &writeError));
        CHECK(writeError.empty());
        CHECK(prefab->GetRootGuid() == templateRootGuid);
        CHECK(prefab->GetTemplateObjects().size() == templateCountBefore);
        for (const std::shared_ptr<GameObject>& templateObject : prefab->GetTemplateObjects())
        {
            REQUIRE(templateObject != nullptr);
            CHECK_FALSE(PrefabUtility::IsEditorTempStageObject(*templateObject));
        }
    }
}
