#include "Services/Scene/SceneEditorServices.h"

#include "Commands/Scene/EditorAddComponentCommand.h"
#include "Commands/Scene/EditorAddEmptyGameObjectCommand.h"
#include "Commands/Scene/EditorDeleteGameObjectCommand.h"
#include "Commands/Scene/EditorInstantiatePrefabCommand.h"
#include "Commands/Scene/EditorMoveComponentCommand.h"
#include "Commands/Scene/EditorRemoveComponentCommand.h"
#include "Commands/Scene/EditorReparentGameObjectCommand.h"
#include "Commands/Scene/EditorSetGameObjectTransformCommand.h"
#include "Commands/Scene/EditorSetObjectPropertyCommand.h"
#include "Services/Scene/ScenePropertyValueCodec.h"
#include "Shell/Document/EditorDocumentSession.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Prefab/PrefabEditConstraints.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/PropertyAssign.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Transform/Transform.h"

#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace minEngine
{
    SceneEditorResult SceneEditorServices::OkResult()
    {
        SceneEditorResult result;
        result.Ok = true;
        result.Code = SceneEditorError::None;
        return result;
    }

    SceneEditorResult SceneEditorServices::Fail(SceneEditorError code, std::string message)
    {
        SceneEditorResult result;
        result.Ok = false;
        result.Code = code;
        result.Message = std::move(message);
        return result;
    }

    SceneEditorResult SceneEditorServices::EnsureSceneSession(const EditorDocumentSession& session)
    {
        if (session.GetTypeId() != "Scene")
        {
            return Fail(SceneEditorError::InvalidArgument, "Session is not a Scene document.");
        }
        return OkResult();
    }

    SceneEditor* SceneEditorServices::ResolveSceneEditor(IEditorContext& context, SceneEditorResult& outError)
    {
        SceneEditor* sceneEditor = dynamic_cast<SceneEditor*>(context.FindSubModule(SceneEditor::kModuleId));
        if (sceneEditor == nullptr)
        {
            outError = Fail(SceneEditorError::Internal, "SceneEditor module is unavailable.");
            return nullptr;
        }
        return sceneEditor;
    }

    bool SceneEditorServices::TryResolveComponentTypeName(
        SceneEditor& sceneEditor,
        std::string_view className,
        std::string& outResolvedTypeName,
        SceneEditorResult& outError)
    {
        outResolvedTypeName.clear();
        if (className.empty())
        {
            outError = Fail(SceneEditorError::InvalidArgument, "className is empty.");
            return false;
        }

        const std::string requested(className);
        const std::vector<std::string>& typeNames = sceneEditor.GetAllComponentTypeNames();
        for (const std::string& typeName : typeNames)
        {
            if (typeName == requested)
            {
                outResolvedTypeName = typeName;
                return true;
            }
        }

        std::string unambiguous;
        size_t matchCount = 0;
        for (const std::string& typeName : typeNames)
        {
            const size_t pos = typeName.rfind(requested);
            if (pos == std::string::npos)
            {
                continue;
            }
            const bool suffixMatch =
                (pos + requested.size() == typeName.size())
                && (pos == 0 || typeName[pos - 1] == ':');
            if (!suffixMatch)
            {
                continue;
            }
            ++matchCount;
            unambiguous = typeName;
        }

        if (matchCount == 1)
        {
            outResolvedTypeName = unambiguous;
            return true;
        }

        if (matchCount == 0)
        {
            outError = Fail(SceneEditorError::NotFound, "Unknown component type: " + requested);
        }
        else
        {
            outError = Fail(SceneEditorError::InvalidArgument, "Ambiguous component type: " + requested);
        }
        return false;
    }

    SceneEditorResult SceneEditorServices::GetScene(
        IEditorContext& context,
        EditorDocumentSession& session,
        Scene** outScene) const
    {
        if (outScene == nullptr)
        {
            return Fail(SceneEditorError::InvalidArgument, "outScene is null.");
        }
        *outScene = nullptr;

        SceneEditorResult sessionCheck = EnsureSceneSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        SceneEditorResult error = OkResult();
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }

        // MVP binding: session selects command stack; Scene is the Level document scene.
        Scene* scene = sceneEditor->GetDocumentScene();
        if (scene == nullptr)
        {
            return Fail(SceneEditorError::NotFound, "No document Scene is open.");
        }

        *outScene = scene;
        return OkResult();
    }

    SceneEditorResult SceneEditorServices::FindGameObject(
        Scene& scene,
        const GUID& id,
        GameObject** outObject) const
    {
        if (outObject == nullptr)
        {
            return Fail(SceneEditorError::InvalidArgument, "outObject is null.");
        }
        *outObject = nullptr;

        for (const std::shared_ptr<GameObject>& gameObject : scene.GetAllGameObjects())
        {
            if (gameObject && gameObject->GetGuid() == id)
            {
                *outObject = gameObject.get();
                return OkResult();
            }
        }

        return Fail(SceneEditorError::NotFound, "No GameObject with this GUID in scene.");
    }

    SceneEditorResult SceneEditorServices::FindComponent(
        Scene& scene,
        const GUID& componentId,
        Component** outComponent,
        GameObject** outOwner) const
    {
        if (outComponent == nullptr)
        {
            return Fail(SceneEditorError::InvalidArgument, "outComponent is null.");
        }
        *outComponent = nullptr;
        if (outOwner != nullptr)
        {
            *outOwner = nullptr;
        }

        for (const std::shared_ptr<GameObject>& gameObject : scene.GetAllGameObjects())
        {
            if (!gameObject)
            {
                continue;
            }
            for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
            {
                if (component && component->GetGuid() == componentId)
                {
                    *outComponent = component.get();
                    if (outOwner != nullptr)
                    {
                        *outOwner = gameObject.get();
                    }
                    return OkResult();
                }
            }
        }

        return Fail(SceneEditorError::NotFound, "No Component with this GUID in scene.");
    }

    SceneEditorResult SceneEditorServices::GetProperty(
        MEObject& owner,
        std::string_view propertyPath,
        void* outValue,
        size_t outValueSize) const
    {
        if (outValue == nullptr)
        {
            return Fail(SceneEditorError::InvalidArgument, "outValue is null.");
        }

        const Reflection::MEClass* ownerClass = owner.GetClass();
        if (ownerClass == nullptr)
        {
            return Fail(SceneEditorError::Internal, "Owner class is null.");
        }

        const std::string normalizedPath = ScenePropertyValueCodec::NormalizePropertyPath(propertyPath);
        void* leafOwner = nullptr;
        const Reflection::MEClass* leafOwnerClass = nullptr;
        std::string leafPropertyName;
        std::string walkError;
        if (!ScenePropertyValueCodec::WalkToLeafOwner(
                &owner,
                ownerClass,
                normalizedPath,
                leafOwner,
                leafOwnerClass,
                leafPropertyName,
                walkError))
        {
            return Fail(SceneEditorError::InvalidPropertyPath, walkError);
        }

        const Reflection::MEProperty* property = nullptr;
        Reflection::ReflectionSystem::Get().ForEachPropertyInHierarchy(
            leafOwnerClass->GetName(),
            [&](const Reflection::MEProperty& candidate) -> bool
            {
                if (candidate.GetName() == leafPropertyName)
                {
                    property = &candidate;
                    return false;
                }
                return true;
            });
        if (property == nullptr)
        {
            return Fail(SceneEditorError::InvalidPropertyPath, "Leaf property not found.");
        }

        const size_t storageSize = property->GetStorageSize();
        if (outValueSize < storageSize)
        {
            return Fail(SceneEditorError::InvalidArgument, "outValueSize is smaller than property storage.");
        }

        if (!Reflection::GetPropertyValue(leafOwner, *property, outValue))
        {
            return Fail(SceneEditorError::Internal, "GetPropertyValue failed.");
        }
        return OkResult();
    }

    SceneEditorResult SceneEditorServices::SetProperty(
        IEditorContext& context,
        EditorDocumentSession& session,
        MEObject& owner,
        std::string_view propertyPath,
        const void* value,
        size_t valueSize)
    {
        if (value == nullptr)
        {
            return Fail(SceneEditorError::InvalidArgument, "value is null.");
        }

        SceneEditorResult sessionCheck = EnsureSceneSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        SceneEditorResult error = OkResult();
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }

        const Reflection::MEClass* ownerClass = owner.GetClass();
        if (ownerClass == nullptr)
        {
            return Fail(SceneEditorError::Internal, "Owner class is null.");
        }

        const std::string normalizedPath = ScenePropertyValueCodec::NormalizePropertyPath(propertyPath);
        const Serialization::SerializerOptions options = sceneEditor->GetPropertyCommandSerializerOptions();

        void* leafOwner = nullptr;
        const Reflection::MEClass* leafOwnerClass = nullptr;
        std::string leafPropertyName;
        std::string walkError;
        if (!ScenePropertyValueCodec::WalkToLeafOwner(
                &owner,
                ownerClass,
                normalizedPath,
                leafOwner,
                leafOwnerClass,
                leafPropertyName,
                walkError))
        {
            return Fail(SceneEditorError::InvalidPropertyPath, walkError);
        }

        const Reflection::MEProperty* property = nullptr;
        Reflection::ReflectionSystem::Get().ForEachPropertyInHierarchy(
            leafOwnerClass->GetName(),
            [&](const Reflection::MEProperty& candidate) -> bool
            {
                if (candidate.GetName() == leafPropertyName)
                {
                    property = &candidate;
                    return false;
                }
                return true;
            });
        if (property == nullptr)
        {
            return Fail(SceneEditorError::InvalidPropertyPath, "Leaf property not found.");
        }

        const size_t storageSize = property->GetStorageSize();
        if (valueSize != storageSize)
        {
            return Fail(SceneEditorError::InvalidArgument, "valueSize does not match property storage size.");
        }

        std::vector<uint8_t> beforeBlob;
        std::string codecError;
        if (!ScenePropertyValueCodec::TryCaptureBlob(
                owner, *ownerClass, normalizedPath, beforeBlob, codecError, options))
        {
            return Fail(SceneEditorError::Internal, codecError);
        }

        Reflection::PropertyAssignOptions assignOptions;
        assignOptions.notifyPostEdit = true;
        assignOptions.postEditObject = &owner;
        assignOptions.postEditPropertyName = leafPropertyName;
        if (!Reflection::AssignProperty(leafOwner, *property, value, assignOptions))
        {
            return Fail(SceneEditorError::Internal, "AssignProperty failed.");
        }

        std::vector<uint8_t> afterBlob;
        if (!ScenePropertyValueCodec::TryCaptureBlob(
                owner, *ownerClass, normalizedPath, afterBlob, codecError, options))
        {
            ScenePropertyValueCodec::TryRestoreBlob(
                owner, *ownerClass, normalizedPath, beforeBlob, codecError, options);
            return Fail(SceneEditorError::Internal, codecError);
        }

        if (!ScenePropertyValueCodec::TryRestoreBlob(
                owner, *ownerClass, normalizedPath, beforeBlob, codecError, options))
        {
            return Fail(SceneEditorError::Internal, "Failed to restore pre-edit property value: " + codecError);
        }

        if (beforeBlob == afterBlob)
        {
            return OkResult();
        }

        session.GetCommandStack().Execute(std::make_unique<EditorSetObjectPropertyCommand>(
            *sceneEditor,
            owner.GetGuid(),
            ownerClass->GetName(),
            normalizedPath,
            std::move(beforeBlob),
            std::move(afterBlob),
            true));
        return OkResult();
    }

    SceneEditorResult SceneEditorServices::CommitObjectPropertyBlobs(
        IEditorContext& context,
        EditorDocumentSession& session,
        const GUID& ownerGuid,
        std::string_view ownerClassName,
        std::string_view propertyPath,
        std::vector<uint8_t> beforeValue,
        std::vector<uint8_t> afterValue,
        bool applyOnFirstExecute)
    {
        SceneEditorResult sessionCheck = EnsureSceneSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        if (beforeValue == afterValue)
        {
            return OkResult();
        }

        SceneEditorResult error = OkResult();
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }

        const std::string normalizedPath = ScenePropertyValueCodec::NormalizePropertyPath(propertyPath);
        session.GetCommandStack().Execute(std::make_unique<EditorSetObjectPropertyCommand>(
            *sceneEditor,
            ownerGuid,
            std::string(ownerClassName),
            normalizedPath,
            std::move(beforeValue),
            std::move(afterValue),
            applyOnFirstExecute));
        return OkResult();
    }

    SceneEditorResult SceneEditorServices::CreateGameObject(
        IEditorContext& context,
        EditorDocumentSession& session,
        std::string_view name,
        GameObject* parent,
        GameObject** outCreated)
    {
        if (outCreated == nullptr)
        {
            return Fail(SceneEditorError::InvalidArgument, "outCreated is null.");
        }
        *outCreated = nullptr;

        Scene* scene = nullptr;
        SceneEditorResult sceneResult = GetScene(context, session, &scene);
        if (!sceneResult.Ok)
        {
            return sceneResult;
        }

        SceneEditorResult error = OkResult();
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }

        if (parent == nullptr)
        {
            std::string constraintError;
            if (!PrefabEditConstraints::AllowAddTopLevelGameObject(*sceneEditor, &constraintError))
            {
                return Fail(SceneEditorError::PrefabConstrained, constraintError);
            }
        }

        std::unordered_set<std::string> beforeGuids;
        for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
        {
            if (gameObject)
            {
                beforeGuids.insert(gameObject->GetGuid().ToString());
            }
        }

        session.GetCommandStack().Execute(std::make_unique<EditorAddEmptyGameObjectCommand>(*sceneEditor));

        GameObject* created = nullptr;
        for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
        {
            if (!gameObject)
            {
                continue;
            }
            if (beforeGuids.find(gameObject->GetGuid().ToString()) == beforeGuids.end())
            {
                created = gameObject.get();
                break;
            }
        }

        if (created == nullptr)
        {
            return Fail(SceneEditorError::Internal, "Create GameObject command did not produce a new object.");
        }

        if (!name.empty() && created->GetName() != name)
        {
            std::string nameStorage(name);
            SceneEditorResult renameResult = SetProperty(
                context,
                session,
                *created,
                "m_Name",
                &nameStorage,
                sizeof(nameStorage));
            if (!renameResult.Ok)
            {
                return renameResult;
            }
        }

        if (parent != nullptr)
        {
            SceneEditorResult reparentResult = Reparent(context, session, *created, parent);
            if (!reparentResult.Ok)
            {
                return reparentResult;
            }
        }

        *outCreated = created;
        return OkResult();
    }

    SceneEditorResult SceneEditorServices::DeleteGameObject(
        IEditorContext& context,
        EditorDocumentSession& session,
        GameObject& object)
    {
        SceneEditorResult sessionCheck = EnsureSceneSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        SceneEditorResult error = OkResult();
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }

        std::string constraintError;
        if (!PrefabEditConstraints::AllowDeleteGameObject(*sceneEditor, object.GetID(), &constraintError))
        {
            return Fail(SceneEditorError::PrefabConstrained, constraintError);
        }

        session.GetCommandStack().Execute(
            std::make_unique<EditorDeleteGameObjectCommand>(*sceneEditor, object.GetID()));
        return OkResult();
    }

    SceneEditorResult SceneEditorServices::AddComponent(
        IEditorContext& context,
        EditorDocumentSession& session,
        GameObject& object,
        std::string_view typeName,
        Component** outAdded)
    {
        if (outAdded == nullptr)
        {
            return Fail(SceneEditorError::InvalidArgument, "outAdded is null.");
        }
        *outAdded = nullptr;

        SceneEditorResult sessionCheck = EnsureSceneSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        SceneEditorResult error = OkResult();
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }

        std::string constraintError;
        if (!PrefabEditConstraints::AllowAddComponent(*sceneEditor, object.GetID(), &constraintError))
        {
            return Fail(SceneEditorError::PrefabConstrained, constraintError);
        }

        std::string resolvedTypeName;
        if (!TryResolveComponentTypeName(*sceneEditor, typeName, resolvedTypeName, error))
        {
            return error;
        }

        std::unordered_set<std::string> beforeComponentGuids;
        for (const std::shared_ptr<Component>& component : object.GetAllComponents())
        {
            if (component)
            {
                beforeComponentGuids.insert(component->GetGuid().ToString());
            }
        }

        session.GetCommandStack().Execute(std::make_unique<EditorAddComponentCommand>(
            *sceneEditor, object.GetID(), resolvedTypeName));

        for (const std::shared_ptr<Component>& component : object.GetAllComponents())
        {
            if (!component)
            {
                continue;
            }
            if (beforeComponentGuids.find(component->GetGuid().ToString()) == beforeComponentGuids.end())
            {
                *outAdded = component.get();
                return OkResult();
            }
        }

        return Fail(SceneEditorError::Internal, "AddComponent command did not produce a new component.");
    }

    SceneEditorResult SceneEditorServices::RemoveComponent(
        IEditorContext& context,
        EditorDocumentSession& session,
        Component& component)
    {
        SceneEditorResult sessionCheck = EnsureSceneSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        SceneEditorResult error = OkResult();
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }

        GameObject* owner = component.GetOwner();
        if (owner == nullptr)
        {
            return Fail(SceneEditorError::InvalidArgument, "Component has no owner GameObject.");
        }

        std::string constraintError;
        if (!PrefabEditConstraints::AllowRemoveComponent(
                *sceneEditor, owner->GetID(), component.GetGuid(), &constraintError))
        {
            return Fail(SceneEditorError::PrefabConstrained, constraintError);
        }

        session.GetCommandStack().Execute(
            std::make_unique<EditorRemoveComponentCommand>(*sceneEditor, owner->GetID(), component));
        return OkResult();
    }

    SceneEditorResult SceneEditorServices::Reparent(
        IEditorContext& context,
        EditorDocumentSession& session,
        GameObject& object,
        GameObject* newParent)
    {
        SceneEditorResult sessionCheck = EnsureSceneSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        SceneEditorResult error = OkResult();
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }

        const uint64_t newParentId =
            newParent != nullptr ? newParent->GetID() : SceneEditor::kSceneRootParentId;

        std::string constraintError;
        if (!PrefabEditConstraints::AllowReparentGameObject(
                *sceneEditor, object.GetID(), newParentId, &constraintError))
        {
            return Fail(SceneEditorError::PrefabConstrained, constraintError);
        }

        if (newParent != nullptr)
        {
            for (GameObject* walk = newParent; walk != nullptr; walk = walk->GetParent())
            {
                if (walk->GetID() == object.GetID())
                {
                    return Fail(SceneEditorError::InvalidArgument, "Cannot reparent under own descendant.");
                }
            }
        }

        const uint64_t oldParentId = object.GetParent() != nullptr
            ? object.GetParent()->GetID()
            : SceneEditor::kSceneRootParentId;
        if (oldParentId == newParentId)
        {
            return OkResult();
        }

        session.GetCommandStack().Execute(std::make_unique<EditorReparentGameObjectCommand>(
            *sceneEditor, object.GetID(), oldParentId, newParentId));
        return OkResult();
    }

    SceneEditorResult SceneEditorServices::MoveComponent(
        IEditorContext& context,
        EditorDocumentSession& session,
        GameObject& owner,
        Component& component,
        size_t newIndex)
    {
        SceneEditorResult sessionCheck = EnsureSceneSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        if (component.GetOwner() != &owner)
        {
            return Fail(SceneEditorError::InvalidArgument, "Component does not belong to owner GameObject.");
        }

        SceneEditorResult error = OkResult();
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }

        const size_t fromIndex = owner.FindComponentIndex(component);
        if (fromIndex >= owner.GetAllComponents().size() || fromIndex == newIndex)
        {
            return OkResult();
        }

        session.GetCommandStack().Execute(std::make_unique<EditorMoveComponentCommand>(
            *sceneEditor, owner.GetID(), component.GetGuid(), fromIndex, newIndex));
        return OkResult();
    }

    SceneEditorResult SceneEditorServices::SetTransform(
        IEditorContext& context,
        EditorDocumentSession& session,
        GameObject& object,
        const Transform& before,
        const Transform& after)
    {
        SceneEditorResult sessionCheck = EnsureSceneSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        if (before == after)
        {
            return OkResult();
        }

        SceneEditorResult error = OkResult();
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }

        session.GetCommandStack().Execute(std::make_unique<EditorSetGameObjectTransformCommand>(
            *sceneEditor, object.GetID(), before, after));
        return OkResult();
    }

    SceneEditorResult SceneEditorServices::InstantiatePrefab(
        IEditorContext& context,
        EditorDocumentSession& session,
        std::shared_ptr<Prefab> prefab,
        GameObject* attachParent)
    {
        if (!prefab)
        {
            return Fail(SceneEditorError::InvalidArgument, "prefab is null.");
        }

        SceneEditorResult sessionCheck = EnsureSceneSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        SceneEditorResult error = OkResult();
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }

        if (context.IsPlaying())
        {
            return Fail(SceneEditorError::InvalidArgument, "Instantiate Prefab is unavailable during Play.");
        }

        std::string constraintError;
        if (!PrefabEditConstraints::AllowInstantiatePrefab(*sceneEditor, &constraintError))
        {
            return Fail(SceneEditorError::PrefabConstrained, constraintError);
        }

        session.GetCommandStack().Execute(std::make_unique<EditorInstantiatePrefabCommand>(
            *sceneEditor, std::move(prefab), attachParent));
        return OkResult();
    }

    SceneEditorResult SceneEditorServices::SaveDocument(
        IEditorContext& context,
        EditorDocumentSession& session)
    {
        SceneEditorResult sessionCheck = EnsureSceneSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        SceneEditorResult error = OkResult();
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }

        if (!sceneEditor->SaveCurrentScene(context))
        {
            return Fail(SceneEditorError::Internal, "Failed to save scene document.");
        }
        return OkResult();
    }

    SceneEditorResult SceneEditorServices::Validate(
        IEditorContext& /*context*/,
        EditorDocumentSession& session,
        MEObject* /*object*/,
        std::string_view /*propertyPath*/) const
    {
        SceneEditorResult sessionCheck = EnsureSceneSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        // Honest P0 stub: always ok when session is a Scene document.
        return OkResult();
    }
}
