#include "SceneCloneTest.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneCloneContext.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Function/Framework/Scene/SceneDuplicator.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Physics/RigidBodyComponent.h"
#include "Runtime/Function/Physics/BoxColliderComponent.h"

#include <cstring>
#include <filesystem>
#include <fstream>

namespace minEngine
{
    class SceneCloneTestScope
    {
    public:
        SceneCloneTestScope()
        {
            ObjectManager::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();

            SceneManager::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~SceneCloneTestScope()
        {
            m_SceneManager.Shutdown();
            SceneManager::SetInstance(nullptr);

            m_ObjectManager.Shutdown();
            ObjectManager::SetInstance(nullptr);
        }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
    };

    namespace
    {
        bool RunSceneCloneAttachHierarchyTest()
        {
            SceneCloneTestScope scope;

            const std::shared_ptr<Scene> editorScene = SceneManager::Get().CreateNewScene("scene-clone");
            if (!editorScene)
            {
                ME_CORE_ERROR("SceneCloneTest: failed to create editor scene.");
                return false;
            }

            const std::shared_ptr<GameObject> parentObject = editorScene->CreateGameObject();
            const std::shared_ptr<SceneComponent> parentComponent = parentObject->AddComponent<SceneComponent>();
            parentComponent->SetPosition(Vector3(1.0f, 2.0f, 3.0f));

            const std::shared_ptr<GameObject> childObject = editorScene->CreateGameObject();
            const std::shared_ptr<SceneComponent> childComponent = childObject->AddComponent<SceneComponent>();
            if (!childComponent->AttachToComponent(parentComponent.get(), AttachmentTransformRules::KeepRelativeTransform))
            {
                ME_CORE_ERROR("SceneCloneTest: failed to attach child to parent.");
                return false;
            }

            const GUID editorParentGuid = parentComponent->GetGuid();
            const GUID editorChildGuid = childComponent->GetGuid();

            SceneCloneContext cloneContext;
            const std::shared_ptr<Scene> pieScene = SceneDuplicator::DuplicateForPIE(*editorScene, cloneContext);
            if (!pieScene)
            {
                ME_CORE_ERROR("SceneCloneTest: DuplicateForPIE returned null.");
                return false;
            }

            if (pieScene->GetAllGameObjects().size() != editorScene->GetAllGameObjects().size())
            {
                ME_CORE_ERROR("SceneCloneTest: game object count mismatch after clone.");
                return false;
            }

            const auto parentIter = cloneContext.SourceToClonedGuid.find(editorParentGuid);
            const auto childIter = cloneContext.SourceToClonedGuid.find(editorChildGuid);
            if (parentIter == cloneContext.SourceToClonedGuid.end() || childIter == cloneContext.SourceToClonedGuid.end())
            {
                ME_CORE_ERROR("SceneCloneTest: clone context missing component GUID mapping.");
                return false;
            }

            if (parentIter->second == editorParentGuid || childIter->second == editorChildGuid)
            {
                ME_CORE_ERROR("SceneCloneTest: cloned GUIDs must differ from editor GUIDs.");
                return false;
            }

            SceneComponent* pieParent = nullptr;
            SceneComponent* pieChild = nullptr;
            for (const std::shared_ptr<GameObject>& gameObject : pieScene->GetAllGameObjects())
            {
                if (!gameObject)
                {
                    continue;
                }

                for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
                {
                    SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component.get());
                    if (sceneComponent == nullptr)
                    {
                        continue;
                    }

                    if (sceneComponent->GetGuid() == parentIter->second)
                    {
                        pieParent = sceneComponent;
                    }
                    else if (sceneComponent->GetGuid() == childIter->second)
                    {
                        pieChild = sceneComponent;
                    }
                }
            }

            if (pieParent == nullptr || pieChild == nullptr)
            {
                ME_CORE_ERROR("SceneCloneTest: failed to locate cloned components in PIE scene.");
                return false;
            }

            if (pieChild->GetAttachParent() != pieParent)
            {
                ME_CORE_ERROR("SceneCloneTest: attach parent was not remapped in PIE scene.");
                return false;
            }

            if (pieParent->GetAttachChildren().size() != 1 || pieParent->GetAttachChildren()[0] != pieChild)
            {
                ME_CORE_ERROR("SceneCloneTest: attach children were not rebuilt in PIE scene.");
                return false;
            }

            return true;
        }

        bool RunSceneAttachParentSerializationRoundTripTest()
        {
            SceneCloneTestScope scope;

            const std::shared_ptr<Scene> sourceScene = SceneManager::Get().CreateNewScene("attach-serialize");
            const std::shared_ptr<GameObject> parentObject = sourceScene->CreateGameObject();
            const std::shared_ptr<SceneComponent> parentComponent = parentObject->AddComponent<SceneComponent>();
            const std::shared_ptr<GameObject> childObject = sourceScene->CreateGameObject();
            const std::shared_ptr<SceneComponent> childComponent = childObject->AddComponent<SceneComponent>();
            if (!childComponent->AttachToComponent(parentComponent.get(), AttachmentTransformRules::KeepRelativeTransform))
            {
                ME_CORE_ERROR("SceneCloneTest: failed to attach child for serialization round-trip.");
                return false;
            }

            std::vector<uint8_t> buffer;
            const Serialization::SerializeResult serializeResult = Serialization::Serializer::SerializeObjectToBuffer(
                "minEngine::Scene",
                sourceScene.get(),
                buffer);
            if (!serializeResult.ok)
            {
                ME_CORE_ERROR("SceneCloneTest: scene serialize failed.");
                return false;
            }

            std::shared_ptr<Scene> loadedScene = NewObject<Scene>();
            loadedScene->m_SceneName = "attach-serialize-loaded";
            ObjectManager::Get().UnregisterObject(loadedScene.get());

            std::vector<Serialization::PendingObjectRef> deserializeRefs;
            const Serialization::SerializeResult deserializeResult = Serialization::Serializer::DeserializeObjectFromBuffer(
                "minEngine::Scene",
                loadedScene.get(),
                buffer,
                deserializeRefs);
            if (!deserializeResult.ok)
            {
                ME_CORE_ERROR("SceneCloneTest: scene deserialize failed.");
                return false;
            }

            const Serialization::SerializeResult resolveResult =
                Serialization::Serializer::ResolvePendingObjectRefs(deserializeRefs);
            if (!resolveResult.ok)
            {
                ME_CORE_ERROR("SceneCloneTest: scene deserialize resolve failed.");
                return false;
            }

            ObjectManager::Get().RegisterObject(loadedScene);
            loadedScene->SetSceneType(ESceneType::Editor);
            SceneManager::FinalizeLoadedScene(loadedScene.get());

            SceneComponent* loadedParent = nullptr;
            SceneComponent* loadedChild = nullptr;
            for (const std::shared_ptr<GameObject>& gameObject : loadedScene->GetAllGameObjects())
            {
                if (!gameObject)
                {
                    continue;
                }

                for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
                {
                    SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component.get());
                    if (sceneComponent == nullptr)
                    {
                        continue;
                    }

                    if (sceneComponent->GetGuid() == parentComponent->GetGuid())
                    {
                        loadedParent = sceneComponent;
                    }
                    else if (sceneComponent->GetGuid() == childComponent->GetGuid())
                    {
                        loadedChild = sceneComponent;
                    }
                }
            }

            if (loadedParent == nullptr || loadedChild == nullptr)
            {
                ME_CORE_ERROR("SceneCloneTest: loaded scene missing attach components.");
                return false;
            }

            if (loadedChild->GetAttachParent() != loadedParent)
            {
                ME_CORE_ERROR("SceneCloneTest: loaded scene attach parent mismatch.");
                return false;
            }

            if (loadedParent->GetAttachChildren().size() != 1 || loadedParent->GetAttachChildren()[0] != loadedChild)
            {
                ME_CORE_ERROR("SceneCloneTest: loaded scene attach children mismatch.");
                return false;
            }

            return true;
        }

        bool RunLegacySceneFileWithoutAttachParentTest()
        {
            SceneCloneTestScope scope;

            const std::filesystem::path tempScenePath =
                std::filesystem::temp_directory_path() / "minengine_legacy_scene_no_attach_parent.mescene";
            const std::string legacySceneJson = R"({
    "m_SceneName": "legacy-no-attach-parent",
    "m_GameObjects": [
        {
            "$ptr_typeName": "minEngine::GameObject",
            "m_Components": [
                {
                    "$ptr_typeName": "minEngine::SceneComponent",
                    "m_Guid": { "High": 1, "Low": 1 },
                    "m_Name": "",
                    "m_Transform": {
                        "Position": [0.0, 0.0, 0.0],
                        "Rotation": { "W": 1.0, "X": 0.0, "Y": 0.0, "Z": 0.0 },
                        "Scale": [1.0, 1.0, 1.0]
                    },
                    "m_bActive": true
                }
            ],
            "m_Guid": { "High": 2, "Low": 2 },
            "m_Name": "Root"
        }
    ]
})";

            {
                std::ofstream output(tempScenePath);
                if (!output.is_open())
                {
                    ME_CORE_ERROR("SceneCloneTest: failed to create temp legacy scene file.");
                    return false;
                }
                output << legacySceneJson;
            }

            std::shared_ptr<Scene> scene = NewObject<Scene>("legacy-no-attach-parent");
            scene->Reset();
            ObjectManager::Get().UnregisterObject(scene.get());

            Serialization::JsonReaderArchive archive;
            const Serialization::SerializeResult loadResult = Serialization::Serializer::FromFile(
                tempScenePath.string(),
                "minEngine::Scene",
                scene.get(),
                archive,
                Serialization::SerializerOptions{
                    .enumAsString = true,
                    .strictTypeCheck = true,
                    .skipUnknownField = true,
                    .allowObjectPtrSerialization = true,
                });
            std::error_code removeError;
            std::filesystem::remove(tempScenePath, removeError);

            if (!loadResult.ok)
            {
                ME_CORE_ERROR(
                    "SceneCloneTest: legacy scene without m_AttachParent failed to load: {} (field: {})",
                    loadResult.message,
                    loadResult.fieldPath);
                return false;
            }

            SceneManager::FinalizeLoadedScene(scene.get());
            if (scene->GetAllGameObjects().size() != 1)
            {
                ME_CORE_ERROR("SceneCloneTest: legacy scene game object count mismatch.");
                return false;
            }

            const std::shared_ptr<GameObject>& loadedObject = scene->GetAllGameObjects()[0];
            SceneComponent* sceneComponent = nullptr;
            for (const std::shared_ptr<Component>& component : loadedObject->GetAllComponents())
            {
                sceneComponent = dynamic_cast<SceneComponent*>(component.get());
                if (sceneComponent != nullptr)
                {
                    break;
                }
            }

            if (sceneComponent == nullptr)
            {
                ME_CORE_ERROR("SceneCloneTest: legacy scene scene component missing.");
                return false;
            }

            if (sceneComponent->GetAttachParent() != nullptr)
            {
                ME_CORE_ERROR("SceneCloneTest: legacy scene attach parent should remain null.");
                return false;
            }

            return true;
        }

        bool RunSceneDuplicatePhysicsStackTest()
        {
            SceneCloneTestScope scope;

            const std::shared_ptr<Scene> editorScene = SceneManager::Get().CreateNewScene("test");
            if (!editorScene)
            {
                ME_CORE_ERROR("SceneCloneTest: failed to create physics-stack editor scene.");
                return false;
            }

            const auto addPhysicsMeshGO = [&](const char* name, EBodyType bodyType)
            {
                const std::shared_ptr<GameObject> gameObject = editorScene->CreateGameObject();
                gameObject->Rename(name);
                gameObject->AddComponent<StaticMeshComponent>();
                gameObject->AddComponent<RigidBodyComponent>()->SetBodyType(bodyType);
                gameObject->AddComponent<BoxColliderComponent>();
            };

            addPhysicsMeshGO("Cube", EBodyType::Dynamic);
            addPhysicsMeshGO("plane", EBodyType::Static);

            SceneCloneContext cloneContext;
            const std::shared_ptr<Scene> pieScene = SceneDuplicator::DuplicateForPIE(*editorScene, cloneContext);
            if (!pieScene)
            {
                ME_CORE_ERROR("SceneCloneTest: DuplicateForPIE failed on physics-stack scene.");
                return false;
            }

            if (pieScene->GetAllGameObjects().size() != editorScene->GetAllGameObjects().size())
            {
                ME_CORE_ERROR("SceneCloneTest: physics-stack scene game object count mismatch after PIE clone.");
                return false;
            }

            return true;
        }
    }

    bool RunSceneCloneTests()
    {
        return RunSceneCloneAttachHierarchyTest()
            && RunSceneAttachParentSerializationRoundTripTest()
            && RunLegacySceneFileWithoutAttachParentTest()
            && RunSceneDuplicatePhysicsStackTest();
    }
}

#include "doctest.h"

#include "EngineTestFixture.h"

TEST_CASE("scene-clone: attach hierarchy remaps for PIE [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunSceneCloneTests());
}
