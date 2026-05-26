#include "SubEditor/Scene/SceneEditor.h"

#include "Commands/Scene/DeleteGameObjectCommand.h"
#include "Commands/Scene/AddEmptyGameObjectCommand.h"
#include "Commands/Scene/AddComponentCommand.h"
#include "Commands/Scene/RemoveComponentCommand.h"
#include "Commands/Scene/RenameGameObjectCommand.h"
#include "Commands/Scene/SetGameObjectTransformCommand.h"
#include "Commands/Scene/EditorObjectSnapshot.h"
#include "Commands/Scene/SetObjectPropertyCommand.h"
#include "EditorGUIManager.h"
#include "SubEditor/Scene/SceneEditorInspectorSource.h"
#include "Shell/EditorCommandStack.h"
#include "Shell/IEditorContext.h"

#include "UI/EditorWindows/HierarchyWindow.h"
#include "UI/EditorWindows/SceneEditingViewportWindow.h"
#include "Shell/EditorDockLayout.h"
#include "Shell/EditorInputHub.h"

#include "imgui.h"
#include "SubEditor/Scene/SceneEditingViewportClient.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
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

        context.GetCommandStack().Execute(std::make_unique<RemoveComponentCommand>(*this, gameObject.GetID(), targetComponent));
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

    void SceneEditor::SubmitRemoveGameObjectFromScene(IEditorContext& context, uint64_t gameObjectId)
    {
        context.GetCommandStack().Execute(std::make_unique<DeleteGameObjectCommand>(*this, gameObjectId));
    }

    bool SceneEditor::ApplySetObjectProperty(const GUID& ownerGuid,
                                             const std::string& ownerClassName,
                                             const std::string& propertyPath,
                                             const std::vector<uint8_t>& valueBlob)
    {
        std::shared_ptr<MEObject> ownerObject = ObjectManager::Get().FindObject(ownerGuid);
        if (!ownerObject)
        {
            ME_CORE_WARN(
                "ApplySetObjectProperty: owner not found (guid='{}', property='{}').",
                ownerGuid.ToString(),
                propertyPath);
            return false;
        }

        const Reflection::MEClass* ownerClass = Reflection::ReflectionSystem::Get().FindClass(ownerClassName);
        if (ownerClass == nullptr)
        {
            ME_CORE_WARN(
                "ApplySetObjectProperty: class '{}' not found (property='{}').",
                ownerClassName,
                propertyPath);
            return false;
        }

        std::vector<Serialization::PendingObjectRef> unresolvedRefs;
        const Serialization::SerializeResult result = Serialization::Serializer::DeserializePropertyByPathFromBuffer(
            ownerObject.get(),
            ownerClass,
            propertyPath,
            valueBlob,
            unresolvedRefs,
            GetPropertyCommandSerializerOptions());
        if (!result.ok)
        {
            ME_CORE_WARN(
                "ApplySetObjectProperty failed: {} (path='{}').",
                result.message,
                result.fieldPath);
            return false;
        }

        if (!unresolvedRefs.empty())
        {
            const Serialization::SerializeResult resolveResult =
                Serialization::Serializer::ResolvePendingObjectRefs(unresolvedRefs);
            if (!resolveResult.ok)
            {
                ME_CORE_WARN("ApplySetObjectProperty: unresolved object references remain.");
            }
        }

        if (ownerObject->IsA(SceneComponent::StaticClass()))
        {
            static_cast<SceneComponent*>(ownerObject.get())->MarkRenderStateDirty();
        }

        MarkSceneDirty();
        return true;
    }

    void SceneEditor::SubmitSetObjectProperty(IEditorContext& context,
                                              const GUID& ownerGuid,
                                              const std::string& ownerClassName,
                                              const std::string& propertyPath,
                                              std::vector<uint8_t> beforeValue,
                                              std::vector<uint8_t> afterValue)
    {
        if (beforeValue == afterValue)
        {
            return;
        }

        context.GetCommandStack().Execute(std::make_unique<SetObjectPropertyCommand>(
            *this,
            ownerGuid,
            ownerClassName,
            propertyPath,
            std::move(beforeValue),
            std::move(afterValue)));
    }

    bool SceneEditor::TryCaptureGameObjectSnapshotForDelete(uint64_t gameObjectId,
                                                            EditorObjectSnapshot& outSnapshot,
                                                            std::string& outDescription)
    {
        Scene* scene = GetActiveScene();
        if (scene == nullptr)
        {
            return false;
        }

        GameObject* gameObject = scene->FindGameObjectById(gameObjectId);
        if (gameObject == nullptr)
        {
            return false;
        }

        outDescription = std::string("Delete GameObject '") + gameObject->GetName() + "'";

        const Serialization::SerializeResult captureResult = CaptureGameObjectSnapshot(*gameObject, outSnapshot);
        if (!captureResult.ok)
        {
            ME_CORE_ERROR(
                "TryCaptureGameObjectSnapshotForDelete: capture failed: {} (path='{}').",
                captureResult.message,
                captureResult.fieldPath);
            return false;
        }

        return true;
    }

    bool SceneEditor::TryCaptureComponentSnapshotForRemove(uint64_t ownerGameObjectId,
                                                           const GUID& componentGuid,
                                                           EditorObjectSnapshot& outSnapshot,
                                                           int32_t& outComponentIndex,
                                                           std::string& outDescription)
    {
        Scene* scene = GetActiveScene();
        if (scene == nullptr)
        {
            return false;
        }

        GameObject* owner = scene->FindGameObjectById(ownerGameObjectId);
        if (owner == nullptr)
        {
            return false;
        }

        Component* component = nullptr;
        outComponentIndex = -1;
        const std::vector<std::shared_ptr<Component>>& components = owner->GetAllComponents();
        for (size_t index = 0; index < components.size(); ++index)
        {
            if (components[index] && components[index]->GetGuid() == componentGuid)
            {
                component = components[index].get();
                outComponentIndex = static_cast<int32_t>(index);
                break;
            }
        }

        if (component == nullptr || outComponentIndex < 0)
        {
            return false;
        }

        if (const Reflection::MEClass* classInfo = component->GetClass())
        {
            outDescription = std::string("Remove Component '") + classInfo->GetName() + "'";
        }

        const Serialization::SerializeResult captureResult =
            CaptureComponentSnapshot(*component, *owner, outComponentIndex, outSnapshot);
        if (!captureResult.ok)
        {
            ME_CORE_ERROR(
                "TryCaptureComponentSnapshotForRemove: capture failed: {} (path='{}').",
                captureResult.message,
                captureResult.fieldPath);
            return false;
        }

        return true;
    }

    bool SceneEditor::ApplyRemoveComponentByGuid(uint64_t ownerGameObjectId, const GUID& componentGuid)
    {
        Scene* scene = GetActiveScene();
        if (scene == nullptr)
        {
            return false;
        }

        GameObject* owner = scene->FindGameObjectById(ownerGameObjectId);
        if (owner == nullptr)
        {
            return false;
        }

        for (const std::shared_ptr<Component>& componentPtr : owner->GetAllComponents())
        {
            if (componentPtr && componentPtr->GetGuid() == componentGuid)
            {
                return ApplyRemoveComponentFromGO(*owner, *componentPtr);
            }
        }

        return false;
    }

    Serialization::SerializeResult SceneEditor::CaptureGameObjectSnapshot(const GameObject& gameObject,
                                                                          EditorObjectSnapshot& outSnapshot) const
    {
        const Reflection::MEClass* rootClass = gameObject.GetClass();
        if (rootClass == nullptr)
        {
            return Serialization::SerializeResult::Failure("CaptureGameObjectSnapshot: GameObject class is null.");
        }

        outSnapshot = EditorObjectSnapshot{};
        outSnapshot.kind = EditorSnapshotKind::GameObject;
        outSnapshot.sourceRuntimeId = gameObject.GetID();
        outSnapshot.sourceRootGuid = gameObject.GetGuid();
        outSnapshot.rootClassName = rootClass->GetName();

        return Serialization::Serializer::SerializeObjectToBuffer(
            outSnapshot.rootClassName,
            &gameObject,
            outSnapshot.payload);
    }

    Serialization::SerializeResult SceneEditor::CaptureComponentSnapshot(const Component& component,
                                                                           GameObject& owner,
                                                                           int32_t componentIndex,
                                                                           EditorObjectSnapshot& outSnapshot) const
    {
        const Reflection::MEClass* rootClass = component.GetClass();
        if (rootClass == nullptr)
        {
            return Serialization::SerializeResult::Failure("CaptureComponentSnapshot: component class is null.");
        }

        outSnapshot = EditorObjectSnapshot{};
        outSnapshot.kind = EditorSnapshotKind::Component;
        outSnapshot.sourceRootGuid = component.GetGuid();
        outSnapshot.rootClassName = rootClass->GetName();
        outSnapshot.ownerGameObjectId = owner.GetID();
        outSnapshot.ownerGameObjectGuid = owner.GetGuid();
        outSnapshot.componentIndexInOwner = componentIndex;

        return Serialization::Serializer::SerializeObjectToBuffer(
            outSnapshot.rootClassName,
            &component,
            outSnapshot.payload);
    }

    Serialization::SerializerOptions SceneEditor::GetRestoreSerializerOptions()
    {
        Serialization::SerializerOptions options;
        options.skipUnknownField = false;
        options.allowObjectPtrSerialization = true;
        return options;
    }

    Serialization::SerializerOptions SceneEditor::GetPropertyCommandSerializerOptions() const
    {
        return GetRestoreSerializerOptions();
    }

    void SceneEditor::PostRestoreSceneObject(GameObject& gameObject)
    {
        if (gameObject.GetRootComponent() == nullptr)
        {
            for (const std::shared_ptr<Component>& componentPtr : gameObject.GetAllComponents())
            {
                if (!componentPtr || !componentPtr->GetClass()
                    || !componentPtr->IsA(SceneComponent::StaticClass()))
                {
                    continue;
                }

                gameObject.SetRootComponent(static_cast<SceneComponent*>(componentPtr.get()));
                break;
            }
        }

        for (const std::shared_ptr<Component>& componentPtr : gameObject.GetAllComponents())
        {
            if (!componentPtr)
            {
                continue;
            }

            if (componentPtr->GetOwner() == nullptr)
            {
                componentPtr->SetOwner(&gameObject);
            }

            if (SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(componentPtr.get()))
            {
                sceneComponent->MarkRenderStateDirty();
            }

            SceneManager::Get().MarkComponentForNeededEndOfFrameUpdate(componentPtr.get());
        }

        MarkSceneDirty();
    }

    uint64_t SceneEditor::ApplyRestoreGameObjectFromSnapshot(const EditorObjectSnapshot& snapshot)
    {
        if (snapshot.kind != EditorSnapshotKind::GameObject || snapshot.rootClassName.empty() || snapshot.payload.empty())
        {
            ME_CORE_ERROR("ApplyRestoreGameObjectFromSnapshot: invalid snapshot.");
            return std::numeric_limits<uint64_t>::max();
        }

        Scene* scene = GetActiveScene();
        if (scene == nullptr)
        {
            ME_CORE_ERROR("ApplyRestoreGameObjectFromSnapshot: no active scene.");
            return std::numeric_limits<uint64_t>::max();
        }

        const Reflection::MEClass* rootClass = Reflection::ReflectionSystem::Get().FindClass(snapshot.rootClassName);
        if (rootClass == nullptr)
        {
            ME_CORE_ERROR("ApplyRestoreGameObjectFromSnapshot: class '{}' not found.", snapshot.rootClassName);
            return std::numeric_limits<uint64_t>::max();
        }

        std::shared_ptr<void> instanceVoid = rootClass->CreateDefaultInstance();
        if (!instanceVoid)
        {
            ME_CORE_ERROR("ApplyRestoreGameObjectFromSnapshot: CreateDefaultInstance failed.");
            return std::numeric_limits<uint64_t>::max();
        }

        std::shared_ptr<GameObject> gameObject = std::static_pointer_cast<GameObject>(instanceVoid);
        gameObject->SetOuter(scene);

        const Serialization::SerializerOptions restoreOptions = GetRestoreSerializerOptions();
        std::vector<Serialization::PendingObjectRef> unresolvedRefs;
        const Serialization::SerializeResult deserializeResult = Serialization::Serializer::DeserializeObjectFromBuffer(
            snapshot.rootClassName,
            gameObject.get(),
            snapshot.payload,
            unresolvedRefs,
            restoreOptions);
        if (!deserializeResult.ok)
        {
            ME_CORE_ERROR(
                "ApplyRestoreGameObjectFromSnapshot: deserialize failed: {} (path='{}').",
                deserializeResult.message,
                deserializeResult.fieldPath);
            return std::numeric_limits<uint64_t>::max();
        }

        ObjectManager::Get().RegisterObject(std::static_pointer_cast<MEObject>(gameObject));

        if (!scene->InsertRestoredGameObject(gameObject))
        {
            ME_CORE_ERROR("ApplyRestoreGameObjectFromSnapshot: InsertRestoredGameObject failed.");
            ObjectManager::Get().UnregisterObject(gameObject.get());
            return std::numeric_limits<uint64_t>::max();
        }

        if (!unresolvedRefs.empty())
        {
            const Serialization::SerializeResult resolveResult =
                Serialization::Serializer::ResolvePendingObjectRefs(unresolvedRefs);
            if (!resolveResult.ok)
            {
                ME_CORE_WARN("ApplyRestoreGameObjectFromSnapshot: some pending references remain unresolved.");
            }
        }

        PostRestoreSceneObject(*gameObject);
        return gameObject->GetID();
    }

    Component* SceneEditor::ApplyRestoreComponentFromSnapshot(uint64_t ownerGameObjectId,
                                                              const EditorObjectSnapshot& snapshot)
    {
        if (snapshot.kind != EditorSnapshotKind::Component || snapshot.rootClassName.empty() || snapshot.payload.empty())
        {
            ME_CORE_ERROR("ApplyRestoreComponentFromSnapshot: invalid snapshot.");
            return nullptr;
        }

        Scene* scene = GetActiveScene();
        if (scene == nullptr)
        {
            ME_CORE_ERROR("ApplyRestoreComponentFromSnapshot: no active scene.");
            return nullptr;
        }

        GameObject* owner = scene->FindGameObjectById(ownerGameObjectId);
        if (owner == nullptr && !snapshot.ownerGameObjectGuid.IsZero())
        {
            const std::shared_ptr<MEObject> ownerObject = FindObject(snapshot.ownerGameObjectGuid);
            owner = dynamic_cast<GameObject*>(ownerObject.get());
        }

        if (owner == nullptr)
        {
            ME_CORE_ERROR("ApplyRestoreComponentFromSnapshot: owner GameObject not found.");
            return nullptr;
        }

        const Reflection::MEClass* rootClass = Reflection::ReflectionSystem::Get().FindClass(snapshot.rootClassName);
        if (rootClass == nullptr)
        {
            ME_CORE_ERROR("ApplyRestoreComponentFromSnapshot: class '{}' not found.", snapshot.rootClassName);
            return nullptr;
        }

        std::shared_ptr<void> instanceVoid = rootClass->CreateDefaultInstance();
        if (!instanceVoid)
        {
            ME_CORE_ERROR("ApplyRestoreComponentFromSnapshot: CreateDefaultInstance failed.");
            return nullptr;
        }

        std::shared_ptr<Component> component = std::static_pointer_cast<Component>(instanceVoid);
        component->SetOuter(owner);

        const Serialization::SerializerOptions restoreOptions = GetRestoreSerializerOptions();
        std::vector<Serialization::PendingObjectRef> unresolvedRefs;
        const Serialization::SerializeResult deserializeResult = Serialization::Serializer::DeserializeObjectFromBuffer(
            snapshot.rootClassName,
            component.get(),
            snapshot.payload,
            unresolvedRefs,
            restoreOptions);
        if (!deserializeResult.ok)
        {
            ME_CORE_ERROR(
                "ApplyRestoreComponentFromSnapshot: deserialize failed: {} (path='{}').",
                deserializeResult.message,
                deserializeResult.fieldPath);
            return nullptr;
        }

        ObjectManager::Get().RegisterObject(std::static_pointer_cast<MEObject>(component));

        if (!unresolvedRefs.empty())
        {
            const Serialization::SerializeResult resolveResult =
                Serialization::Serializer::ResolvePendingObjectRefs(unresolvedRefs);
            if (!resolveResult.ok)
            {
                ME_CORE_WARN("ApplyRestoreComponentFromSnapshot: some pending references remain unresolved.");
            }
        }

        const size_t insertIndex = snapshot.componentIndexInOwner >= 0
            ? static_cast<size_t>(snapshot.componentIndexInOwner)
            : owner->GetAllComponents().size();
        owner->InsertRestoredComponent(component, insertIndex);

        if (SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component.get()))
        {
            sceneComponent->MarkRenderStateDirty();
        }
        SceneManager::Get().MarkComponentForNeededEndOfFrameUpdate(component.get());
        MarkSceneDirty();

        return component.get();
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
