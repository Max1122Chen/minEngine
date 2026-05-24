#include "Scene/SceneEditor.h"

#include "Commands/Scene/DeleteGameObjectCommand.h"
#include "Commands/Scene/AddEmptyGameObjectCommand.h"
#include "Commands/Scene/AddComponentCommand.h"
#include "Commands/Scene/RemoveComponentCommand.h"
#include "Commands/Scene/RenameGameObjectCommand.h"
#include "Commands/Scene/SetGameObjectTransformCommand.h"
#include "EditorGUIManager.h"
#include "Scene/SceneEditorInspectorSource.h"
#include "Shell/EditorCommandStack.h"
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
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/Transform/Transform.h"
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
        m_Context = &context;
        EditorGUIManager& gui = context.GetGUIManager();

        gui.RegisterWindow(std::make_unique<SceneEditingViewportWindow>(context));
        gui.RegisterWindow(std::make_unique<HierarchyWindow>(context));
    }

    void SceneEditor::Shutdown()
    {
        m_Context = nullptr;
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

    bool SceneEditor::LoadScene(IEditorContext& context, const std::string& sceneName)
    {
        if (!SceneManager::Get().LoadScene(sceneName))
        {
            return false;
        }

        context.GetCommandStack().Clear();
        SyncSelectionWithScene();
        ClearSceneDirty();
        return true;
    }

    bool SceneEditor::ApplyRenameGameObject(uint64_t gameObjectId, const std::string& newName)
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

    void SceneEditor::SubmitRenameGameObject(IEditorContext& context,
                                             uint64_t gameObjectId,
                                             const std::string& newName)
    {
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            return;
        }

        const std::unordered_map<uint64_t, GameObject*>& gameObjectsById = scene->GetGameObjectsById();
        const auto iter = gameObjectsById.find(gameObjectId);
        if (iter == gameObjectsById.end() || iter->second == nullptr)
        {
            return;
        }

        const std::string& oldName = iter->second->GetName();
        std::string sanitizedName = newName;
        if (sanitizedName.empty())
        {
            sanitizedName = "GameObject_" + std::to_string(gameObjectId);
        }

        if (oldName == sanitizedName)
        {
            return;
        }

        context.GetCommandStack().Execute(std::make_unique<RenameGameObjectCommand>(
            *this, gameObjectId, oldName, sanitizedName));
    }

    void SceneEditor::ApplyGameObjectTransform(uint64_t gameObjectId, const Transform& transform)
    {
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            return;
        }

        const std::unordered_map<uint64_t, GameObject*>& gameObjectsById = scene->GetGameObjectsById();
        const auto iter = gameObjectsById.find(gameObjectId);
        if (iter == gameObjectsById.end() || iter->second == nullptr)
        {
            return;
        }

        GameObject* gameObject = iter->second;
        const Transform current = gameObject->GetTransform();
        if (current == transform)
        {
            return;
        }

        gameObject->SetTransform(transform);
        MarkSceneDirty();
    }

    void SceneEditor::SubmitGameObjectTransform(IEditorContext& context,
                                                uint64_t gameObjectId,
                                                const Transform& before,
                                                const Transform& after)
    {
        if (before == after)
        {
            return;
        }

        context.GetCommandStack().Execute(std::make_unique<SetGameObjectTransformCommand>(
            *this, gameObjectId, before, after));
    }

    const std::vector<std::string>& SceneEditor::GetAllComponentTypeNames() const
    {
        return m_AllComponentTypeNames;
    }

    bool SceneEditor::ApplyAddComponentToSelectedGameObject(const std::string& componentTypeName, Component*& outNewComponent)
    {
        outNewComponent = nullptr;
        GameObject* gameObject = GetSelectedGameObject();
        if (!gameObject)
        {
            return false;
        }
        std::shared_ptr<Component> newComponent = gameObject->AddComponent(componentTypeName);
        if (!newComponent)
        {
            ME_CORE_ERROR(
                "Failed to add component of type '{}' to GameObject '{}'.",
                componentTypeName,
                gameObject->GetName());
            return false;
        }
        outNewComponent = newComponent.get();
        MarkSceneDirty();
        return true;
    }

    void SceneEditor::SubmitAddComponentToSelectedGameObject(IEditorContext& context, const std::string& componentTypeName)
    {
        GameObject* gameObject = GetSelectedGameObject();
        if (!gameObject)
        {
            return;
        }

        context.GetCommandStack().Execute(std::make_unique<AddComponentCommand>(
            *this, gameObject->GetID(), componentTypeName));
    }

    bool SceneEditor::ApplyRemoveComponentFromGO(GameObject& gameObject, Component& targetComponent)
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

    void SceneEditor::SubmitRemoveComponentFromGO(IEditorContext& context, GameObject& gameObject, Component& targetComponent)
    {
        if (targetComponent.GetOwner() != &gameObject)
        {
            return;
        }

        const Reflection::MEClass* classInfo = targetComponent.GetClass();
        if (!classInfo)
        {
            return;
        }

        context.GetCommandStack().Execute(std::make_unique<RemoveComponentCommand>(
            *this, gameObject.GetID(), classInfo->GetName()));
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

    uint64_t SceneEditor::ApplyAddEmptyGOToScene()
    {
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            ME_CORE_ERROR("No active scene to add GameObject to.");
            return std::numeric_limits<uint64_t>::max();
        }
        std::shared_ptr<GameObject> newGO = scene->CreateGameObject();
        if (newGO)
        {
            newGO->Rename("GameObject");
            MarkSceneDirty();
            SelectGameObject(newGO->GetID());
            ME_CORE_INFO("Added new GameObject '{}' to scene '{}'.", newGO->GetName(), scene->GetSceneName());
            return newGO->GetID();
        }
        else
        {
            ME_CORE_ERROR("Failed to create new GameObject in scene '{}'.", scene->GetSceneName());
            return std::numeric_limits<uint64_t>::max();
        }
    }

    void SceneEditor::SubmitAddEmptyGOToScene(IEditorContext& context)
    {
        context.GetCommandStack().Execute(std::make_unique<AddEmptyGameObjectCommand>(*this));
    }

    bool SceneEditor::ApplyRemoveGameObjectFromScene(uint64_t gameObjectId, std::string& outName, Transform& outTransform)
    {
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            ME_CORE_ERROR("No active scene to remove GameObject from.");
            return false;
        }

        GameObject* gameObject = scene->FindGameObjectById(gameObjectId);
        if (!gameObject)
        {
            return false;
        }

        outName = gameObject->GetName();
        outTransform = gameObject->GetTransform();

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

    uint64_t SceneEditor::ApplyRestoreRemovedGameObject(const std::string& name, const Transform& transform)
    {
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            ME_CORE_ERROR("No active scene to restore GameObject to.");
            return std::numeric_limits<uint64_t>::max();
        }

        std::shared_ptr<GameObject> newGO = scene->CreateGameObject();
        if (!newGO)
        {
            ME_CORE_ERROR("Failed to restore GameObject in scene '{}'.", scene->GetSceneName());
            return std::numeric_limits<uint64_t>::max();
        }

        newGO->Rename(name);
        newGO->SetTransform(transform);
        MarkSceneDirty();
        SelectGameObject(newGO->GetID());
        return newGO->GetID();
    }

    void SceneEditor::SubmitRemoveGameObjectFromScene(IEditorContext& context, uint64_t gameObjectId)
    {
        context.GetCommandStack().Execute(std::make_unique<DeleteGameObjectCommand>(*this, gameObjectId));
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
