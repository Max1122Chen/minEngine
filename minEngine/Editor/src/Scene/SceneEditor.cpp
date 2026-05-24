#include "Scene/SceneEditor.h"

#include "EditorGUIManager.h"
#include "Scene/SceneEditorInspectorSource.h"
#include "Shell/IEditorContext.h"

#include "UI/EditorWindows/HierarchyWindow.h"
#include "UI/EditorWindows/SceneEditingViewportWindow.h"
#include "Shell/EditorDockLayout.h"
#include "Shell/EditorInputHub.h"

#include "imgui.h"
#include "Viewport/SceneEditingViewportClient.h"

#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Resource/AssetMeta.h"

#include <algorithm>

namespace minEngine
{
    SceneEditor::SceneEditor()
        : m_InspectorSource(*this)
    {
    }

    void SceneEditor::Register(IEditorContext& context)
    {
        EditorGUIManager& gui = context.GetGUIManager();

        gui.RegisterWindow(std::make_unique<SceneEditingViewportWindow>(context));
        gui.RegisterWindow(std::make_unique<HierarchyWindow>(context));
    }

    void SceneEditor::Shutdown()
    {
    }

    void SceneEditor::OnActivate(IEditorContext& context)
    {
        (void)context;
        SyncSelectionWithScene();
    }

    void SceneEditor::OnDeactivate(IEditorContext& context)
    {
        (void)context;
    }

    void SceneEditor::RegisterCommands(IEditorContext& context)
    {
        EditorCommandBinding saveSceneCommand;
        saveSceneCommand.Name = "Save Scene";
        saveSceneCommand.Chord = { ImGuiKey_S, true, false, false };
        saveSceneCommand.CanExecute = [this]() { return GetActiveScene() != nullptr; };
        saveSceneCommand.Execute = [this]() { SaveCurrentScene(); };
        context.GetInputHub().RegisterActiveSubModuleCommand(std::move(saveSceneCommand));
    }

    void SceneEditor::UnregisterCommands(IEditorContext& context)
    {
        context.GetInputHub().ClearActiveSubModuleCommands();
    }

    void SceneEditor::Tick(float deltaTime)
    {
        (void)deltaTime;
    }

    void SceneEditor::ApplyDefaultLayout(IEditorContext& context, ImGuiID dockspaceId)
    {
        (void)context;
        EditorDockLayout::BuildSceneEditingLayout(dockspaceId);
    }

    bool SceneEditor::OpenAsset(const AssetMeta& meta)
    {
        (void)meta;
        return false;
    }

    bool SceneEditor::RouteViewportInput(EditorViewportClient& client)
    {
        (void)client;
        return dynamic_cast<SceneEditingViewportClient*>(&client) != nullptr;
    }

    void SceneEditor::OnProjectOpened()
    {
        SyncSelectionWithScene();
    }

    void SceneEditor::InitializeComponentTypeNames()
    {
        m_AllComponentTypeNames.clear();
        Reflection::ReflectionSystem& reflectionSystem = Reflection::ReflectionSystem::Get();
        const std::vector<const Reflection::MEClass*>& allClasses = reflectionSystem.GetAllClasses();
        for (const Reflection::MEClass* classInfo : allClasses)
        {
            if (classInfo->IsA(reflectionSystem.FindClass<Component>()))
            {
                m_AllComponentTypeNames.push_back(classInfo->GetName());
            }
        }
    }

    Scene* SceneEditor::GetActiveScene() const
    {
        return SceneManager::Get().GetCurrentActiveScene().get();
    }

    std::vector<GameObject*> SceneEditor::GetHierarchyGameObjects() const
    {
        std::vector<GameObject*> result;
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            return result;
        }

        const std::vector<std::shared_ptr<GameObject>>& gameObjects = scene->GetAllGameObjects();
        result.reserve(gameObjects.size());
        for (const std::shared_ptr<GameObject>& gameObject : gameObjects)
        {
            if (gameObject)
            {
                result.push_back(gameObject.get());
            }
        }

        std::sort(result.begin(), result.end(), [](const GameObject* lhs, const GameObject* rhs)
        {
            return lhs->GetID() < rhs->GetID();
        });

        return result;
    }

    GameObject* SceneEditor::GetSelectedGameObject() const
    {
        return m_SelectedGameObject;
    }

    bool SceneEditor::HasSelectedGameObject() const
    {
        return GetSelectedGameObject() != nullptr;
    }

    void SceneEditor::SelectGameObject(uint64_t gameObjectId)
    {
        m_SelectedGameObjectId = gameObjectId;
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
            m_SelectedGameObject = nullptr;
            return;
        }

        m_SelectedGameObject = scene->FindGameObjectById(gameObjectId);
        if (!m_SelectedGameObject)
        {
            m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
        }
    }

    void SceneEditor::ClearSelectedGameObject()
    {
        m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
        m_SelectedGameObject = nullptr;
    }

    bool SceneEditor::IsGameObjectSelected(uint64_t gameObjectId) const
    {
        return m_SelectedGameObjectId == gameObjectId;
    }

    std::string SceneEditor::GetGameObjectDisplayName(const GameObject& gameObject) const
    {
        return gameObject.GetName();
    }

    std::string SceneEditor::GetSelectedGameObjectName() const
    {
        GameObject* gameObject = GetSelectedGameObject();
        if (!gameObject)
        {
            return std::string();
        }

        return gameObject->GetName();
    }

    bool SceneEditor::RenameGameObject(uint64_t gameObjectId, const std::string& newName)
    {
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            return false;
        }

        const std::unordered_map<uint64_t, GameObject*>& gameObjectsById = scene->GetGameObjectsById();
        const auto iter = gameObjectsById.find(gameObjectId);
        if (iter == gameObjectsById.end() || iter->second == nullptr)
        {
            return false;
        }

        GameObject* gameObject = iter->second;
        std::string sanitizedName = newName;
        if (sanitizedName.empty())
        {
            sanitizedName = "GameObject_" + std::to_string(gameObject->GetID());
        }

        if (gameObject->GetName() != sanitizedName)
        {
            gameObject->Rename(sanitizedName);
            MarkSceneDirty();
        }

        return true;
    }

    void SceneEditor::RenameSelectedGameObject(const std::string& newName)
    {
        RenameGameObject(m_SelectedGameObjectId, newName);
    }

    const std::vector<std::string>& SceneEditor::GetAllComponentTypeNames() const
    {
        return m_AllComponentTypeNames;
    }

    bool SceneEditor::AddComponentToSelectedGameObject(const std::string& componentTypeName)
    {
        GameObject* gameObject = GetSelectedGameObject();
        if (!gameObject)
        {
            return false;
        }
        if (!gameObject->AddComponent(componentTypeName))
        {
            ME_CORE_ERROR(
                "Failed to add component of type '{}' to GameObject '{}'.",
                componentTypeName,
                gameObject->GetName());
            return false;
        }
        MarkSceneDirty();
        return true;
    }

    bool SceneEditor::RemoveComponentFromGO(GameObject& gameObject, Component& targetComponent)
    {
        if (targetComponent.GetOwner() != &gameObject)
        {
            ME_CORE_ERROR(
                "Failed to remove component '{}' from GameObject '{}': component does not belong to the specified GameObject.",
                targetComponent.GetClass()->GetName(),
                gameObject.GetName());
            return false;
        }
        if (gameObject.RemoveComponent(targetComponent))
        {
            MarkSceneDirty();
            return true;
        }
        return false;
    }

    void SceneEditor::SaveCurrentScene()
    {
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            ME_CORE_ERROR("No active scene to save.");
            return;
        }
        if (SceneManager::Get().SaveCurrentScene())
        {
            ClearSceneDirty();
            ME_CORE_INFO("Scene '{}' saved successfully.", scene->GetSceneName());
        }
        else
        {
            ME_CORE_ERROR("Failed to save scene '{}'.", scene->GetSceneName());
        }
    }

    void SceneEditor::AddEmptyGOToScene()
    {
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            ME_CORE_ERROR("No active scene to add GameObject to.");
            return;
        }
        std::shared_ptr<GameObject> newGO = scene->CreateGameObject();
        if (newGO)
        {
            newGO->Rename("GameObject");
            MarkSceneDirty();
            SelectGameObject(newGO->GetID());
            ME_CORE_INFO("Added new GameObject '{}' to scene '{}'.", newGO->GetName(), scene->GetSceneName());
        }
        else
        {
            ME_CORE_ERROR("Failed to create new GameObject in scene '{}'.", scene->GetSceneName());
        }
    }

    bool SceneEditor::RemoveGameObjectFromScene(uint64_t gameObjectId)
    {
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            ME_CORE_ERROR("No active scene to remove GameObject from.");
            return false;
        }
        if (scene->RemoveGameObjectById(gameObjectId))
        {
            MarkSceneDirty();
            if (IsGameObjectSelected(gameObjectId))
            {
                ClearSelectedGameObject();
            }
            ME_CORE_INFO("Removed GameObject with ID {} from scene '{}'.", gameObjectId, scene->GetSceneName());
            return true;
        }

        ME_CORE_ERROR("Failed to remove GameObject with ID {} from scene '{}'.", gameObjectId, scene->GetSceneName());
        return false;
    }

    void SceneEditor::SyncSelectionWithScene()
    {
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
            m_SelectedGameObject = nullptr;
            return;
        }

        const std::unordered_map<uint64_t, GameObject*>& gameObjectsById = scene->GetGameObjectsById();
        if (gameObjectsById.empty())
        {
            m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
            m_SelectedGameObject = nullptr;
            return;
        }

        if (gameObjectsById.find(m_SelectedGameObjectId) != gameObjectsById.end())
        {
            m_SelectedGameObject = scene->FindGameObjectById(m_SelectedGameObjectId);
            return;
        }

        const std::vector<GameObject*> hierarchyGameObjects = GetHierarchyGameObjects();
        if (!hierarchyGameObjects.empty())
        {
            SelectGameObject(hierarchyGameObjects.front()->GetID());
        }
        else
        {
            ClearSelectedGameObject();
        }
    }
}
