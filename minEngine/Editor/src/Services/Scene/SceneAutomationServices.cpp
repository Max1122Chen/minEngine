#include "Services/Scene/SceneAutomationServices.h"

#include "Automation/AutomationProtocol.h"
#include "Commands/Scene/EditorAddComponentCommand.h"
#include "Commands/Scene/EditorAddEmptyGameObjectCommand.h"
#include "Commands/Scene/EditorDeleteGameObjectCommand.h"
#include "Commands/Scene/EditorRemoveComponentCommand.h"
#include "Commands/Scene/EditorReparentGameObjectCommand.h"
#include "Commands/Scene/EditorSetObjectPropertyCommand.h"
#include "Services/Scene/ScenePropertyValueCodec.h"
#include "Shell/Document/EditorDocumentHost.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

#include <limits>

namespace minEngine
{
    SceneServiceResult SceneAutomationServices::MakeError(
        std::string code,
        std::string message,
        Json extra) const
    {
        SceneServiceResult result;
        result.Ok = false;
        result.ErrorCode = std::move(code);
        result.Message = std::move(message);
        result.ExtraErrorFields = std::move(extra);
        return result;
    }

    void SceneAutomationServices::ActivateSession(IEditorContext& context, EditorDocumentSession& session)
    {
        context.GetDocumentHost().Activate(session.GetId());
    }

    EditorDocumentSession* SceneAutomationServices::ResolveSession(
        IEditorContext& context,
        std::string_view sessionId,
        SceneServiceResult& outError)
    {
        uint64_t documentIdValue = 0;
        if (!AutomationProtocol::TryParseSessionId(sessionId, documentIdValue))
        {
            outError.Ok = false;
            outError.ErrorCode = "SessionNotFound";
            outError.Message = "Invalid sessionId.";
            return nullptr;
        }

        EditorDocumentId id;
        id.Value = documentIdValue;
        EditorDocumentSession* session = context.GetDocumentHost().FindSession(id);
        if (session == nullptr)
        {
            outError.Ok = false;
            outError.ErrorCode = "SessionNotFound";
            outError.Message = "Unknown sessionId.";
            return nullptr;
        }

        if (session->GetTypeId() != "Scene")
        {
            outError.Ok = false;
            outError.ErrorCode = "SessionNotFound";
            outError.Message = "Session is not a Scene document.";
            return nullptr;
        }

        return session;
    }

    SceneEditor* SceneAutomationServices::ResolveSceneEditor(IEditorContext& context, SceneServiceResult& outError)
    {
        SceneEditor* sceneEditor = dynamic_cast<SceneEditor*>(context.FindSubModule(SceneEditor::kModuleId));
        if (sceneEditor == nullptr)
        {
            outError.Ok = false;
            outError.ErrorCode = "InternalError";
            outError.Message = "SceneEditor module is unavailable.";
            return nullptr;
        }
        return sceneEditor;
    }

    Scene* SceneAutomationServices::ResolveDocumentScene(SceneEditor& sceneEditor, SceneServiceResult& outError)
    {
        Scene* scene = sceneEditor.GetDocumentScene();
        if (scene == nullptr)
        {
            outError.Ok = false;
            outError.ErrorCode = "InternalError";
            outError.Message = "No document Scene is open.";
            return nullptr;
        }
        return scene;
    }

    std::shared_ptr<MEObject> SceneAutomationServices::ResolveOwner(
        std::string_view objectId,
        SceneServiceResult& outError,
        GUID& outGuid)
    {
        if (!ScenePropertyValueCodec::TryParseGuid(objectId, outGuid))
        {
            outError.Ok = false;
            outError.ErrorCode = "InvalidObjectId";
            outError.Message = "objectId is not a valid GUID.";
            outError.ExtraErrorFields = Json{{"objectId", std::string(objectId)}};
            return nullptr;
        }

        std::shared_ptr<MEObject> owner = ObjectManager::Get().FindObject(outGuid);
        if (!owner)
        {
            outError.Ok = false;
            outError.ErrorCode = "InvalidObjectId";
            outError.Message = "No object with this GUID in session scene.";
            outError.ExtraErrorFields = Json{{"objectId", std::string(objectId)}};
            return nullptr;
        }
        return owner;
    }

    GameObject* SceneAutomationServices::ResolveGameObject(
        Scene& scene,
        std::string_view objectId,
        SceneServiceResult& outError)
    {
        GUID guid;
        if (!ScenePropertyValueCodec::TryParseGuid(objectId, guid))
        {
            outError.Ok = false;
            outError.ErrorCode = "InvalidObjectId";
            outError.Message = "objectId is not a valid GUID.";
            outError.ExtraErrorFields = Json{{"objectId", std::string(objectId)}};
            return nullptr;
        }

        for (const std::shared_ptr<GameObject>& gameObject : scene.GetAllGameObjects())
        {
            if (gameObject && gameObject->GetGuid() == guid)
            {
                return gameObject.get();
            }
        }

        outError.Ok = false;
        outError.ErrorCode = "InvalidObjectId";
        outError.Message = "No GameObject with this GUID in session scene.";
        outError.ExtraErrorFields = Json{{"objectId", std::string(objectId)}};
        return nullptr;
    }

    Component* SceneAutomationServices::ResolveComponent(
        Scene& scene,
        std::string_view componentObjectId,
        GameObject*& outOwner,
        SceneServiceResult& outError)
    {
        outOwner = nullptr;
        GUID guid;
        if (!ScenePropertyValueCodec::TryParseGuid(componentObjectId, guid))
        {
            outError.Ok = false;
            outError.ErrorCode = "InvalidObjectId";
            outError.Message = "componentObjectId is not a valid GUID.";
            outError.ExtraErrorFields = Json{{"componentObjectId", std::string(componentObjectId)}};
            return nullptr;
        }

        for (const std::shared_ptr<GameObject>& gameObject : scene.GetAllGameObjects())
        {
            if (!gameObject)
            {
                continue;
            }
            for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
            {
                if (component && component->GetGuid() == guid)
                {
                    outOwner = gameObject.get();
                    return component.get();
                }
            }
        }

        outError.Ok = false;
        outError.ErrorCode = "InvalidObjectId";
        outError.Message = "No Component with this GUID in session scene.";
        outError.ExtraErrorFields = Json{{"componentObjectId", std::string(componentObjectId)}};
        return nullptr;
    }

    std::unordered_set<std::string> SceneAutomationServices::CollectGameObjectGuidSet(Scene& scene)
    {
        std::unordered_set<std::string> guids;
        for (const std::shared_ptr<GameObject>& gameObject : scene.GetAllGameObjects())
        {
            if (gameObject)
            {
                guids.insert(gameObject->GetGuid().ToString());
            }
        }
        return guids;
    }

    Json SceneAutomationServices::BuildHierarchyNode(GameObject& gameObject, int depthRemaining)
    {
        Json node = {
            {"objectId", gameObject.GetGuid().ToString()},
            {"name", gameObject.GetName()},
            {"children", Json::array()}};

        if (depthRemaining == 0)
        {
            return node;
        }

        for (GameObject* child : gameObject.GetChildren())
        {
            if (child == nullptr)
            {
                continue;
            }
            const int nextDepth = depthRemaining < 0 ? -1 : depthRemaining - 1;
            node["children"].push_back(BuildHierarchyNode(*child, nextDepth));
        }
        return node;
    }

    SceneServiceResult SceneAutomationServices::GetHierarchy(
        IEditorContext& context,
        std::string_view sessionId,
        int maxDepth)
    {
        SceneServiceResult error;
        if (ResolveSession(context, sessionId, error) == nullptr)
        {
            return error;
        }
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }
        Scene* scene = ResolveDocumentScene(*sceneEditor, error);
        if (scene == nullptr)
        {
            return error;
        }

        const int depthRemaining = maxDepth < 0 ? -1 : maxDepth;
        Json roots = Json::array();
        for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
        {
            if (gameObject == nullptr || gameObject->GetParent() != nullptr)
            {
                continue;
            }
            roots.push_back(BuildHierarchyNode(*gameObject, depthRemaining));
        }

        SceneServiceResult result;
        result.Ok = true;
        result.Result = Json{{"roots", std::move(roots)}};
        return result;
    }

    SceneServiceResult SceneAutomationServices::GetObject(
        IEditorContext& context,
        std::string_view sessionId,
        std::string_view objectId)
    {
        SceneServiceResult error;
        if (ResolveSession(context, sessionId, error) == nullptr)
        {
            return error;
        }
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }
        Scene* scene = ResolveDocumentScene(*sceneEditor, error);
        if (scene == nullptr)
        {
            return error;
        }

        GameObject* gameObject = ResolveGameObject(*scene, objectId, error);
        if (gameObject == nullptr)
        {
            return error;
        }

        Json components = Json::array();
        for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
        {
            if (!component || component->GetClass() == nullptr)
            {
                continue;
            }
            components.push_back(Json{
                {"componentObjectId", component->GetGuid().ToString()},
                {"className", component->GetClass()->GetName()}});
        }

        SceneServiceResult result;
        result.Ok = true;
        result.Result = Json{
            {"objectId", gameObject->GetGuid().ToString()},
            {"name", gameObject->GetName()},
            {"components", std::move(components)}};
        return result;
    }

    SceneServiceResult SceneAutomationServices::GetProperty(
        IEditorContext& context,
        std::string_view sessionId,
        std::string_view objectId,
        std::string_view propertyPath)
    {
        SceneServiceResult error;
        if (ResolveSession(context, sessionId, error) == nullptr)
        {
            return error;
        }
        if (ResolveSceneEditor(context, error) == nullptr)
        {
            return error;
        }

        GUID guid;
        std::shared_ptr<MEObject> owner = ResolveOwner(objectId, error, guid);
        if (!owner)
        {
            return error;
        }

        const Reflection::MEClass* ownerClass = owner->GetClass();
        if (ownerClass == nullptr)
        {
            return MakeError("InternalError", "Owner class is null.");
        }

        const std::string normalizedPath = ScenePropertyValueCodec::NormalizePropertyPath(propertyPath);
        Json value;
        std::string codecError;
        if (!ScenePropertyValueCodec::TryReadJson(
                *owner,
                *ownerClass,
                normalizedPath,
                value,
                codecError,
                Serialization::SerializerOptions{}))
        {
            return MakeError(
                "InvalidPropertyPath",
                codecError,
                Json{{"objectId", std::string(objectId)}, {"propertyPath", normalizedPath}});
        }

        SceneServiceResult result;
        result.Ok = true;
        result.Result = Json{
            {"objectId", std::string(objectId)},
            {"propertyPath", normalizedPath},
            {"value", std::move(value)}};
        return result;
    }

    SceneServiceResult SceneAutomationServices::CreateGameObject(
        IEditorContext& context,
        std::string_view sessionId,
        std::string_view name,
        std::string_view parentObjectId)
    {
        SceneServiceResult error;
        EditorDocumentSession* session = ResolveSession(context, sessionId, error);
        if (session == nullptr)
        {
            return error;
        }
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }
        Scene* scene = ResolveDocumentScene(*sceneEditor, error);
        if (scene == nullptr)
        {
            return error;
        }

        GameObject* parent = nullptr;
        if (!parentObjectId.empty())
        {
            parent = ResolveGameObject(*scene, parentObjectId, error);
            if (parent == nullptr)
            {
                return error;
            }
        }

        const std::unordered_set<std::string> beforeGuids = CollectGameObjectGuidSet(*scene);
        session->GetCommandStack().Execute(std::make_unique<EditorAddEmptyGameObjectCommand>(*sceneEditor));
        ActivateSession(context, *session);

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
            return MakeError("InternalError", "Create GameObject command did not produce a new object.");
        }

        if (!name.empty() && created->GetName() != name)
        {
            SceneServiceResult renameResult = SetProperty(
                context,
                sessionId,
                created->GetGuid().ToString(),
                "m_Name",
                Json(std::string(name)));
            if (!renameResult.Ok)
            {
                return renameResult;
            }
        }

        if (parent != nullptr)
        {
            SceneServiceResult reparentResult = Reparent(
                context,
                sessionId,
                created->GetGuid().ToString(),
                parent->GetGuid().ToString());
            if (!reparentResult.Ok)
            {
                return reparentResult;
            }
        }

        SceneServiceResult result;
        result.Ok = true;
        result.Result = Json{
            {"objectId", created->GetGuid().ToString()},
            {"name", created->GetName()}};
        return result;
    }

    SceneServiceResult SceneAutomationServices::DeleteGameObject(
        IEditorContext& context,
        std::string_view sessionId,
        std::string_view objectId)
    {
        SceneServiceResult error;
        EditorDocumentSession* session = ResolveSession(context, sessionId, error);
        if (session == nullptr)
        {
            return error;
        }
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }
        Scene* scene = ResolveDocumentScene(*sceneEditor, error);
        if (scene == nullptr)
        {
            return error;
        }

        GameObject* gameObject = ResolveGameObject(*scene, objectId, error);
        if (gameObject == nullptr)
        {
            return error;
        }

        session->GetCommandStack().Execute(
            std::make_unique<EditorDeleteGameObjectCommand>(*sceneEditor, gameObject->GetID()));
        ActivateSession(context, *session);

        SceneServiceResult result;
        result.Ok = true;
        result.Result = Json{{"deletedObjectId", std::string(objectId)}};
        return result;
    }

    bool SceneAutomationServices::TryResolveComponentTypeName(
        SceneEditor& sceneEditor,
        std::string_view className,
        std::string& outResolvedTypeName,
        SceneServiceResult& outError)
    {
        if (className.empty())
        {
            outError.Ok = false;
            outError.ErrorCode = "InvalidPayload";
            outError.Message = "className is required.";
            return false;
        }

        const std::vector<std::string>& typeNames = sceneEditor.GetAllComponentTypeNames();
        for (const std::string& typeName : typeNames)
        {
            if (typeName == className)
            {
                outResolvedTypeName = typeName;
                return true;
            }
        }

        std::vector<std::string> suffixMatches;
        for (const std::string& typeName : typeNames)
        {
            if (typeName.size() < className.size())
            {
                continue;
            }
            if (typeName.compare(typeName.size() - className.size(), className.size(), className) != 0)
            {
                continue;
            }
            if (typeName.size() > className.size() && typeName[typeName.size() - className.size() - 1] != ':')
            {
                continue;
            }
            suffixMatches.push_back(typeName);
        }

        if (suffixMatches.size() == 1)
        {
            outResolvedTypeName = suffixMatches.front();
            return true;
        }

        outError.Ok = false;
        outError.ErrorCode = "InvalidPayload";
        if (suffixMatches.empty())
        {
            outError.Message = "Unknown component className '" + std::string(className) + "'.";
        }
        else
        {
            outError.Message =
                "Ambiguous component className '" + std::string(className) + "' matches multiple types.";
            outError.ExtraErrorFields = Json{{"matches", suffixMatches}};
        }
        return false;
    }

    SceneServiceResult SceneAutomationServices::AddComponent(
        IEditorContext& context,
        std::string_view sessionId,
        std::string_view objectId,
        std::string_view className)
    {
        SceneServiceResult error;
        EditorDocumentSession* session = ResolveSession(context, sessionId, error);
        if (session == nullptr)
        {
            return error;
        }
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }
        Scene* scene = ResolveDocumentScene(*sceneEditor, error);
        if (scene == nullptr)
        {
            return error;
        }

        std::string resolvedTypeName;
        if (!TryResolveComponentTypeName(*sceneEditor, className, resolvedTypeName, error))
        {
            return error;
        }

        GameObject* gameObject = ResolveGameObject(*scene, objectId, error);
        if (gameObject == nullptr)
        {
            return error;
        }

        std::unordered_set<std::string> beforeComponentGuids;
        for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
        {
            if (component)
            {
                beforeComponentGuids.insert(component->GetGuid().ToString());
            }
        }

        session->GetCommandStack().Execute(std::make_unique<EditorAddComponentCommand>(
            *sceneEditor,
            gameObject->GetID(),
            resolvedTypeName));
        ActivateSession(context, *session);

        std::string createdComponentId;
        for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
        {
            if (!component)
            {
                continue;
            }
            if (beforeComponentGuids.find(component->GetGuid().ToString()) == beforeComponentGuids.end())
            {
                createdComponentId = component->GetGuid().ToString();
                break;
            }
        }

        if (createdComponentId.empty())
        {
            return MakeError(
                "InvalidPayload",
                "Failed to add component '" + resolvedTypeName + "'.");
        }

        SceneServiceResult result;
        result.Ok = true;
        result.Result = Json{
            {"objectId", std::string(objectId)},
            {"componentObjectId", createdComponentId},
            {"className", resolvedTypeName}};
        return result;
    }

    SceneServiceResult SceneAutomationServices::RemoveComponent(
        IEditorContext& context,
        std::string_view sessionId,
        std::string_view componentObjectId)
    {
        SceneServiceResult error;
        EditorDocumentSession* session = ResolveSession(context, sessionId, error);
        if (session == nullptr)
        {
            return error;
        }
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }
        Scene* scene = ResolveDocumentScene(*sceneEditor, error);
        if (scene == nullptr)
        {
            return error;
        }

        GameObject* owner = nullptr;
        Component* component = ResolveComponent(*scene, componentObjectId, owner, error);
        if (component == nullptr || owner == nullptr)
        {
            return error;
        }

        session->GetCommandStack().Execute(
            std::make_unique<EditorRemoveComponentCommand>(*sceneEditor, owner->GetID(), *component));
        ActivateSession(context, *session);

        SceneServiceResult result;
        result.Ok = true;
        result.Result = Json{{"removedComponentObjectId", std::string(componentObjectId)}};
        return result;
    }

    SceneServiceResult SceneAutomationServices::SetProperty(
        IEditorContext& context,
        std::string_view sessionId,
        std::string_view objectId,
        std::string_view propertyPath,
        const Json& value)
    {
        SceneServiceResult error;
        EditorDocumentSession* session = ResolveSession(context, sessionId, error);
        if (session == nullptr)
        {
            return error;
        }

        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }

        GUID guid;
        std::shared_ptr<MEObject> owner = ResolveOwner(objectId, error, guid);
        if (!owner)
        {
            return error;
        }

        const Reflection::MEClass* ownerClass = owner->GetClass();
        if (ownerClass == nullptr)
        {
            return MakeError("InternalError", "Owner class is null.");
        }

        const std::string normalizedPath = ScenePropertyValueCodec::NormalizePropertyPath(propertyPath);
        const Serialization::SerializerOptions options = sceneEditor->GetPropertyCommandSerializerOptions();

        std::vector<uint8_t> beforeBlob;
        std::string codecError;
        if (!ScenePropertyValueCodec::TryCaptureBlob(
                *owner,
                *ownerClass,
                normalizedPath,
                beforeBlob,
                codecError,
                options))
        {
            return MakeError(
                "InvalidPropertyPath",
                codecError,
                Json{{"objectId", std::string(objectId)}, {"propertyPath", normalizedPath}});
        }

        if (!ScenePropertyValueCodec::TryWriteJsonToLive(
                *owner,
                *ownerClass,
                normalizedPath,
                value,
                codecError,
                options))
        {
            ScenePropertyValueCodec::TryRestoreBlob(
                *owner,
                *ownerClass,
                normalizedPath,
                beforeBlob,
                codecError,
                options);
            return MakeError(
                "InvalidPayload",
                codecError,
                Json{{"objectId", std::string(objectId)}, {"propertyPath", normalizedPath}});
        }

        std::vector<uint8_t> afterBlob;
        if (!ScenePropertyValueCodec::TryCaptureBlob(
                *owner,
                *ownerClass,
                normalizedPath,
                afterBlob,
                codecError,
                options))
        {
            ScenePropertyValueCodec::TryRestoreBlob(
                *owner,
                *ownerClass,
                normalizedPath,
                beforeBlob,
                codecError,
                options);
            return MakeError("InternalError", codecError);
        }

        if (!ScenePropertyValueCodec::TryRestoreBlob(
                *owner,
                *ownerClass,
                normalizedPath,
                beforeBlob,
                codecError,
                options))
        {
            return MakeError("InternalError", "Failed to restore pre-edit property value: " + codecError);
        }

        if (beforeBlob == afterBlob)
        {
            SceneServiceResult result;
            result.Ok = true;
            result.Result = Json{
                {"objectId", std::string(objectId)},
                {"propertyPath", normalizedPath},
                {"value", value},
                {"unchanged", true}};
            return result;
        }

        session->GetCommandStack().Execute(std::make_unique<EditorSetObjectPropertyCommand>(
            *sceneEditor,
            guid,
            ownerClass->GetName(),
            normalizedPath,
            std::move(beforeBlob),
            std::move(afterBlob),
            true));
        ActivateSession(context, *session);

        SceneServiceResult result;
        result.Ok = true;
        result.Result = Json{
            {"objectId", std::string(objectId)},
            {"propertyPath", normalizedPath},
            {"value", value}};
        return result;
    }

    SceneServiceResult SceneAutomationServices::Reparent(
        IEditorContext& context,
        std::string_view sessionId,
        std::string_view objectId,
        std::string_view newParentObjectId)
    {
        SceneServiceResult error;
        EditorDocumentSession* session = ResolveSession(context, sessionId, error);
        if (session == nullptr)
        {
            return error;
        }
        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }
        Scene* scene = ResolveDocumentScene(*sceneEditor, error);
        if (scene == nullptr)
        {
            return error;
        }

        GameObject* gameObject = ResolveGameObject(*scene, objectId, error);
        if (gameObject == nullptr)
        {
            return error;
        }

        uint64_t newParentId = SceneEditor::kSceneRootParentId;
        if (!newParentObjectId.empty())
        {
            GameObject* newParent = ResolveGameObject(*scene, newParentObjectId, error);
            if (newParent == nullptr)
            {
                return error;
            }
            newParentId = newParent->GetID();
        }

        const uint64_t oldParentId = gameObject->GetParent() != nullptr
            ? gameObject->GetParent()->GetID()
            : SceneEditor::kSceneRootParentId;
        if (oldParentId == newParentId)
        {
            SceneServiceResult result;
            result.Ok = true;
            result.Result = Json{
                {"objectId", std::string(objectId)},
                {"newParentObjectId", std::string(newParentObjectId)},
                {"unchanged", true}};
            return result;
        }

        session->GetCommandStack().Execute(std::make_unique<EditorReparentGameObjectCommand>(
            *sceneEditor,
            gameObject->GetID(),
            oldParentId,
            newParentId));
        ActivateSession(context, *session);

        SceneServiceResult result;
        result.Ok = true;
        result.Result = Json{
            {"objectId", std::string(objectId)},
            {"newParentObjectId", std::string(newParentObjectId)}};
        return result;
    }

    SceneServiceResult SceneAutomationServices::SaveDocument(
        IEditorContext& context,
        std::string_view sessionId)
    {
        SceneServiceResult error;
        if (ResolveSession(context, sessionId, error) == nullptr)
        {
            return error;
        }

        SceneEditor* sceneEditor = ResolveSceneEditor(context, error);
        if (sceneEditor == nullptr)
        {
            return error;
        }

        if (!sceneEditor->SaveCurrentScene(context))
        {
            return MakeError("InternalError", "Failed to save scene document.");
        }

        SceneServiceResult result;
        result.Ok = true;
        result.Result = Json{{"saved", true}};
        return result;
    }

    SceneServiceResult SceneAutomationServices::Validate(
        IEditorContext& context,
        std::string_view sessionId,
        std::string_view objectId,
        std::string_view propertyPath)
    {
        SceneServiceResult error;
        if (ResolveSession(context, sessionId, error) == nullptr)
        {
            return error;
        }

        Json checks = Json::array();
        checks.push_back(Json{{"name", "session"}, {"ok", true}});

        if (!objectId.empty())
        {
            GUID guid;
            std::shared_ptr<MEObject> owner = ResolveOwner(objectId, error, guid);
            if (!owner)
            {
                return error;
            }
            checks.push_back(Json{{"name", "objectId"}, {"ok", true}});

            if (!propertyPath.empty())
            {
                const Reflection::MEClass* ownerClass = owner->GetClass();
                if (ownerClass == nullptr)
                {
                    return MakeError("InternalError", "Owner class is null.");
                }

                const std::string normalizedPath = ScenePropertyValueCodec::NormalizePropertyPath(propertyPath);
                Json ignored;
                std::string codecError;
                if (!ScenePropertyValueCodec::TryReadJson(
                        *owner,
                        *ownerClass,
                        normalizedPath,
                        ignored,
                        codecError,
                        Serialization::SerializerOptions{}))
                {
                    return MakeError(
                        "InvalidPropertyPath",
                        codecError,
                        Json{{"objectId", std::string(objectId)}, {"propertyPath", normalizedPath}});
                }
                checks.push_back(Json{
                    {"name", "propertyPath"},
                    {"ok", true},
                    {"propertyPath", normalizedPath}});
            }
        }

        SceneServiceResult result;
        result.Ok = true;
        result.Result = Json{
            {"stub", true},
            {"description", "P0 stub: session/object/path resolvability only"},
            {"checks", std::move(checks)}};
        return result;
    }
}
