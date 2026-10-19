#include "Automation/EditorAutomationSurface.h"

#include "Automation/AutomationProtocol.h"
#include "Core.h"
#include "Runtime/Core/EngineVersion.h"
#include "Runtime/Core/Log/LogConsole.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/ProductBranding.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Function/Framework/Project/ProjectManager.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"
#include "Services/Scene/ScenePropertyValueCodec.h"
#include "Services/Asset/AssetEditorServices.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Services/AssetWorkflowModule.h"
#include "Services/Sprite/SpriteEditorServices.h"
#include "Services/Control/EditorControlServices.h"
#include "Services/TypeQuery/TypeQueryServices.h"
#include "Shell/Document/EditorDocumentHost.h"
#include "Shell/Document/EditorDocumentSession.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Scene/SceneEditor.h"

#include <filesystem>
#include <vector>

namespace minEngine
{
    namespace
    {
        SceneEditorResult MakeOk()
        {
            SceneEditorResult result;
            result.Ok = true;
            result.Code = SceneEditorError::None;
            return result;
        }
    }

    EditorAutomationSurface::EditorAutomationSurface(
        CapabilityRegistry& capabilities,
        SceneEditorServices& sceneServices)
        : m_Capabilities(capabilities)
        , m_SceneServices(sceneServices)
    {
        RegisterHandlers();
        AssertHandlersCoverCapabilities();
    }

    void EditorAutomationSurface::RegisterHandlers()
    {
        m_Handlers.clear();

        m_Handlers.emplace(
            "editor.ping",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandlePing(context, request);
            });
        m_Handlers.emplace(
            "editor.get_state",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleGetState(context, request);
            });
        m_Handlers.emplace(
            "editor.get_log",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleGetLog(context, request);
            });
        m_Handlers.emplace(
            "editor.get_capabilities",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleGetCapabilities(context, request);
            });
        m_Handlers.emplace(
            "scene.get_hierarchy",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleGetHierarchy(context, request);
            });
        m_Handlers.emplace(
            "scene.get_object",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleGetObject(context, request);
            });
        m_Handlers.emplace(
            "scene.get_property",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleGetProperty(context, request);
            });
        m_Handlers.emplace(
            "scene.create_game_object",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleCreateGameObject(context, request);
            });
        m_Handlers.emplace(
            "scene.delete_game_object",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleDeleteGameObject(context, request);
            });
        m_Handlers.emplace(
            "scene.add_component",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleAddComponent(context, request);
            });
        m_Handlers.emplace(
            "scene.remove_component",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleRemoveComponent(context, request);
            });
        m_Handlers.emplace(
            "scene.set_property",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleSetProperty(context, request);
            });
        m_Handlers.emplace(
            "scene.reparent",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleReparent(context, request);
            });
        m_Handlers.emplace(
            "scene.save",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleSave(context, request);
            });
        m_Handlers.emplace(
            "scene.validate",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleValidate(context, request);
            });
        m_Handlers.emplace(
            "scene.move_component",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleMoveComponent(context, request);
            });
        m_Handlers.emplace(
            "scene.instantiate_prefab",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleInstantiatePrefab(context, request);
            });
        m_Handlers.emplace(
            "scene.select_object",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleSelectObject(context, request);
            });
        m_Handlers.emplace(
            "reflect.list_classes",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleListClasses(context, request);
            });
        m_Handlers.emplace(
            "reflect.list_properties",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleListProperties(context, request);
            });
        m_Handlers.emplace(
            "asset.list",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleListAssets(context, request);
            });
        m_Handlers.emplace(
            "asset.create",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleCreateAsset(context, request);
            });
        m_Handlers.emplace(
            "asset.delete",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleDeleteAsset(context, request);
            });
        m_Handlers.emplace(
            "asset.list_import_products",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleListImportProducts(context, request);
            });
        m_Handlers.emplace(
            "asset.import",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleImportAsset(context, request);
            });
        m_Handlers.emplace(
            "asset.create_folder",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleCreateFolder(context, request);
            });
        m_Handlers.emplace(
            "asset.move",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleMoveAsset(context, request);
            });
        m_Handlers.emplace(
            "asset.rename",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleRenameAssetOp(context, request);
            });
        m_Handlers.emplace(
            "asset.rename_folder",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleRenameFolder(context, request);
            });
        m_Handlers.emplace(
            "asset.delete_folder",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleDeleteFolder(context, request);
            });
        m_Handlers.emplace(
            "asset.batch_rename",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleBatchRename(context, request);
            });
        m_Handlers.emplace(
            "sprite.create_from_texture",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleSpriteCreateFromTexture(context, request);
            });
        m_Handlers.emplace(
            "sprite.save",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleSpriteSave(context, request);
            });
        m_Handlers.emplace(
            "sprite_atlas.create",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleSpriteAtlasCreate(context, request);
            });
        m_Handlers.emplace(
            "sprite_atlas.add_members",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleSpriteAtlasAddMembers(context, request);
            });
        m_Handlers.emplace(
            "sprite_atlas.set_padding",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleSpriteAtlasSetPadding(context, request);
            });
        m_Handlers.emplace(
            "sprite_atlas.rebuild",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleSpriteAtlasRebuild(context, request);
            });
        m_Handlers.emplace(
            "sprite_atlas.save",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleSpriteAtlasSave(context, request);
            });
        m_Handlers.emplace(
            "sprite_atlas.create_from_selection",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleSpriteAtlasCreateFromSelection(context, request);
            });
        m_Handlers.emplace(
            "asset.open",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleOpenAsset(context, request);
            });
        m_Handlers.emplace(
            "editor.activate_session",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleActivateSession(context, request);
            });
        m_Handlers.emplace(
            "editor.reveal_asset",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleRevealAsset(context, request);
            });
        m_Handlers.emplace(
            "material.get_graph",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleGetMaterialGraph(context, request);
            });
        m_Handlers.emplace(
            "material.add_node",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleMaterialAddNode(context, request);
            });
        m_Handlers.emplace(
            "material.remove_node",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleMaterialRemoveNode(context, request);
            });
        m_Handlers.emplace(
            "material.connect_pins",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleMaterialConnectPins(context, request);
            });
        m_Handlers.emplace(
            "material.disconnect_input",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleMaterialDisconnectInput(context, request);
            });
        m_Handlers.emplace(
            "material.set_property",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleMaterialSetProperty(context, request);
            });
        m_Handlers.emplace(
            "material.save",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleMaterialSave(context, request);
            });
        m_Handlers.emplace(
            "material.compile",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleMaterialCompile(context, request);
            });
        m_Handlers.emplace(
            "animgraph.get_graph",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleGetAnimGraph(context, request);
            });
        m_Handlers.emplace(
            "animgraph.add_state",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleAnimAddState(context, request);
            });
        m_Handlers.emplace(
            "animgraph.remove_state",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleAnimRemoveState(context, request);
            });
        m_Handlers.emplace(
            "animgraph.rename_state",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleAnimRenameState(context, request);
            });
        m_Handlers.emplace(
            "animgraph.add_transition",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleAnimAddTransition(context, request);
            });
        m_Handlers.emplace(
            "animgraph.add_any_state_transition",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleAnimAddAnyStateTransition(context, request);
            });
        m_Handlers.emplace(
            "animgraph.remove_transition",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleAnimRemoveTransition(context, request);
            });
        m_Handlers.emplace(
            "animgraph.remove_any_state_transition",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleAnimRemoveAnyStateTransition(context, request);
            });
        m_Handlers.emplace(
            "animgraph.set_default_state",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleAnimSetDefaultState(context, request);
            });
        m_Handlers.emplace(
            "animgraph.set_property",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleAnimSetProperty(context, request);
            });
        m_Handlers.emplace(
            "animgraph.save",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleAnimSave(context, request);
            });
    }

    void EditorAutomationSurface::AssertHandlersCoverCapabilities() const
    {
        for (const AutomationCapability& capability : m_Capabilities.GetAll())
        {
            if (m_Handlers.find(capability.Op) == m_Handlers.end())
            {
                ME_LOG(
                    LogEditor,
                    Error,
                    "EditorAutomationSurface: capability op '{}' has no handler.",
                    capability.Op);
            }
        }

        for (const auto& [op, handler] : m_Handlers)
        {
            (void)handler;
            if (m_Capabilities.FindByOp(op) == nullptr)
            {
                ME_LOG(
                    LogEditor,
                    Error,
                    "EditorAutomationSurface: handler op '{}' is not in CapabilityRegistry.",
                    op);
            }
        }
    }

    AutomationResponse EditorAutomationSurface::MakeError(
        const AutomationRequest& request,
        const std::string& code,
        const std::string& message,
        Json extraFields) const
    {
        AutomationResponse response;
        response.ProtocolVersion = kAutomationProtocolVersion;
        response.RequestId = request.RequestId;
        response.Ok = false;
        Json error = {{"code", code}, {"message", message}};
        if (!request.Op.empty())
        {
            error["op"] = request.Op;
        }
        for (auto it = extraFields.begin(); it != extraFields.end(); ++it)
        {
            error[it.key()] = it.value();
        }
        response.Errors.push_back(std::move(error));
        return response;
    }

    const char* EditorAutomationSurface::MapErrorCode(SceneEditorError code)
    {
        switch (code)
        {
        case SceneEditorError::None:
            return "Ok";
        case SceneEditorError::NotFound:
            return "InvalidObjectId";
        case SceneEditorError::InvalidArgument:
            return "InvalidPayload";
        case SceneEditorError::InvalidPropertyPath:
            return "InvalidPropertyPath";
        case SceneEditorError::PrefabConstrained:
            return "PrefabConstrained";
        case SceneEditorError::Internal:
        default:
            return "InternalError";
        }
    }

    AutomationResponse EditorAutomationSurface::FromEditorResult(
        const AutomationRequest& request,
        const SceneEditorResult& result,
        Json okResult) const
    {
        if (!result.Ok)
        {
            return MakeError(request, MapErrorCode(result.Code), result.Message);
        }

        AutomationResponse response;
        response.ProtocolVersion = kAutomationProtocolVersion;
        response.RequestId = request.RequestId;
        response.Ok = true;
        response.Result = std::move(okResult);
        return response;
    }

    EditorDocumentSession* EditorAutomationSurface::ResolveSession(
        IEditorContext& context,
        const AutomationRequest& request,
        AutomationResponse& outErrorResponse) const
    {
        uint64_t documentIdValue = 0;
        if (!AutomationProtocol::TryParseSessionId(request.SessionId, documentIdValue))
        {
            outErrorResponse = MakeError(request, "SessionNotFound", "Invalid sessionId.");
            return nullptr;
        }

        EditorDocumentId id;
        id.Value = documentIdValue;
        EditorDocumentSession* session = context.GetDocumentHost().FindSession(id);
        if (session == nullptr)
        {
            outErrorResponse = MakeError(request, "SessionNotFound", "Unknown sessionId.");
            return nullptr;
        }

        if (session->GetTypeId() != "Scene")
        {
            outErrorResponse = MakeError(request, "SessionNotFound", "Session is not a Scene document.");
            return nullptr;
        }

        return session;
    }

    Json EditorAutomationSurface::BuildHierarchyNode(GameObject& gameObject, int depthRemaining) const
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

    AutomationResponse EditorAutomationSurface::Handle(IEditorContext& context, const AutomationRequest& request)
    {
        const auto it = m_Handlers.find(request.Op);
        if (it == m_Handlers.end())
        {
            return MakeError(request, "UnknownOp", "Unknown op: " + request.Op);
        }
        return it->second(context, request);
    }

    AutomationResponse EditorAutomationSurface::HandlePing(
        IEditorContext& /*context*/,
        const AutomationRequest& request) const
    {
        AutomationResponse response;
        response.ProtocolVersion = kAutomationProtocolVersion;
        response.RequestId = request.RequestId;
        response.Ok = true;
        response.Result = Json{
            {"protocolVersion", kAutomationProtocolVersion},
            {"editorProduct", GetProductDisplayName()},
            {"alive", true}};
        return response;
    }

    AutomationResponse EditorAutomationSurface::HandleGetState(
        IEditorContext& context,
        const AutomationRequest& request) const
    {
        AutomationResponse response;
        response.ProtocolVersion = kAutomationProtocolVersion;
        response.RequestId = request.RequestId;
        response.Ok = true;

        std::string projectPath;
        if (ProjectManager::Get().HasOpenProject())
        {
            const ProjectDescriptor& descriptor = ProjectManager::Get().GetCurrentProjectCtx().Descriptor;
            const std::filesystem::path descriptorPath =
                std::filesystem::path(descriptor.ProjectRoot) / (descriptor.ProjectName + ".meproject");
            projectPath = descriptorPath.generic_string();
        }

        Json sessions = Json::array();
        std::string defaultSessionId;
        const EditorDocumentHost& host = context.GetDocumentHost();
        for (const auto& sessionPtr : host.GetSessions())
        {
            if (sessionPtr == nullptr)
            {
                continue;
            }
            const EditorDocumentSession& session = *sessionPtr;
            const std::string sessionId = AutomationProtocol::FormatSessionId(session.GetId().Value);
            Json entry = {
                {"sessionId", sessionId},
                {"documentType", session.GetTypeId()},
                {"assetId", session.GetAssetKey()},
                {"title", session.GetTitle()},
                {"dirty", session.IsDirty()}};
            sessions.push_back(std::move(entry));

            if (defaultSessionId.empty() && session.GetTypeId() == "Scene")
            {
                defaultSessionId = sessionId;
            }
        }

        if (defaultSessionId.empty() && !sessions.empty())
        {
            defaultSessionId = sessions.front()["sessionId"].get<std::string>();
        }

        response.Result = Json{
            {"projectPath", projectPath},
            {"ready", true},
            {"sessions", std::move(sessions)},
            {"defaultSessionId", defaultSessionId}};
        return response;
    }

    AutomationResponse EditorAutomationSurface::HandleGetLog(
        IEditorContext& /*context*/,
        const AutomationRequest& request) const
    {
        size_t limit = 200;
        if (request.Payload.contains("limit"))
        {
            if (!request.Payload["limit"].is_number_integer())
            {
                return MakeError(request, "InvalidPayload", "limit must be an integer.");
            }
            long long rawLimit = request.Payload["limit"].get<long long>();
            if (rawLimit < 1)
            {
                rawLimit = 1;
            }
            if (rawLimit > 500)
            {
                rawLimit = 500;
            }
            limit = static_cast<size_t>(rawLimit);
        }

        LogSeverity minSeverity = LogSeverity::Trace;
        if (request.Payload.contains("minSeverity"))
        {
            if (!request.Payload["minSeverity"].is_string())
            {
                return MakeError(request, "InvalidPayload", "minSeverity must be a string.");
            }
            const std::string severityText = request.Payload["minSeverity"].get<std::string>();
            if (severityText == "Trace")
            {
                minSeverity = LogSeverity::Trace;
            }
            else if (severityText == "Debug")
            {
                minSeverity = LogSeverity::Debug;
            }
            else if (severityText == "Info")
            {
                minSeverity = LogSeverity::Info;
            }
            else if (severityText == "Warn")
            {
                minSeverity = LogSeverity::Warn;
            }
            else if (severityText == "Error")
            {
                minSeverity = LogSeverity::Error;
            }
            else if (severityText == "Fatal")
            {
                minSeverity = LogSeverity::Fatal;
            }
            else
            {
                return MakeError(request, "InvalidPayload", "minSeverity is not a known level.");
            }
        }

        const std::vector<LogRecord> records = LogConsoleStorage::Tail(limit, minSeverity);
        Json entries = Json::array();
        for (const LogRecord& record : records)
        {
            const char* sourceFile = record.source.file != nullptr ? record.source.file : "";
            entries.push_back(Json{
                {"timestamp", LogSystem::FormatLocalTimestamp(record.timestamp)},
                {"channel", record.GetChannelName()},
                {"severity", LogChannelBase::SeverityToString(record.severity)},
                {"message", record.message},
                {"sourceFile", sourceFile},
                {"sourceLine", record.source.line}});
        }

        AutomationResponse response;
        response.ProtocolVersion = kAutomationProtocolVersion;
        response.RequestId = request.RequestId;
        response.Ok = true;
        response.Result = Json{
            {"logFile", LogSystem::GetSessionLogPath().string()},
            {"sessionPointer", LogSystem::GetSessionPointerPath().string()},
            {"ringTruncated", LogConsoleStorage::IsTruncated()},
            {"entries", std::move(entries)}};
        return response;
    }

    AutomationResponse EditorAutomationSurface::HandleGetCapabilities(
        IEditorContext& /*context*/,
        const AutomationRequest& request) const
    {
        AutomationResponse response;
        response.ProtocolVersion = kAutomationProtocolVersion;
        response.RequestId = request.RequestId;
        response.Ok = true;
        response.Result = m_Capabilities.BuildCapabilitiesDocument(GetEngineVersion().ToString());
        return response;
    }

    AutomationResponse EditorAutomationSurface::HandleGetHierarchy(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSession(context, request, errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        Scene* scene = nullptr;
        const SceneEditorResult sceneResult = m_SceneServices.GetScene(context, *session, &scene);
        if (!sceneResult.Ok)
        {
            return FromEditorResult(request, sceneResult);
        }

        int maxDepth = -1;
        if (request.Payload.contains("maxDepth") && request.Payload["maxDepth"].is_number_integer())
        {
            maxDepth = request.Payload["maxDepth"].get<int>();
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

        return FromEditorResult(request, sceneResult, Json{{"roots", std::move(roots)}});
    }

    AutomationResponse EditorAutomationSurface::HandleGetObject(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string objectId = request.Payload.value("objectId", "");
        if (objectId.empty())
        {
            return MakeError(request, "InvalidPayload", "objectId is required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSession(context, request, errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        Scene* scene = nullptr;
        const SceneEditorResult sceneResult = m_SceneServices.GetScene(context, *session, &scene);
        if (!sceneResult.Ok)
        {
            return FromEditorResult(request, sceneResult);
        }

        GUID guid;
        if (!ScenePropertyValueCodec::TryParseGuid(objectId, guid))
        {
            return MakeError(request, "InvalidObjectId", "objectId is not a valid GUID.");
        }

        GameObject* gameObject = nullptr;
        const SceneEditorResult findResult = m_SceneServices.FindGameObject(*scene, guid, &gameObject);
        if (!findResult.Ok)
        {
            return FromEditorResult(request, findResult);
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

        return FromEditorResult(
            request,
            findResult,
            Json{
                {"objectId", gameObject->GetGuid().ToString()},
                {"name", gameObject->GetName()},
                {"components", std::move(components)}});
    }

    AutomationResponse EditorAutomationSurface::HandleGetProperty(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string objectId = request.Payload.value("objectId", "");
        const std::string propertyPath = request.Payload.value("propertyPath", "");
        if (objectId.empty() || propertyPath.empty())
        {
            return MakeError(request, "InvalidPayload", "objectId and propertyPath are required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSession(context, request, errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }
        (void)session;

        GUID guid;
        if (!ScenePropertyValueCodec::TryParseGuid(objectId, guid))
        {
            return MakeError(request, "InvalidObjectId", "objectId is not a valid GUID.");
        }

        std::shared_ptr<MEObject> owner = ObjectManager::Get().FindObject(guid);
        if (!owner)
        {
            return MakeError(request, "InvalidObjectId", "No object with this GUID.");
        }

        const Reflection::MEClass* ownerClass = owner->GetClass();
        if (ownerClass == nullptr)
        {
            return MakeError(request, "InternalError", "Owner class is null.");
        }

        SceneEditor* sceneEditor = dynamic_cast<SceneEditor*>(context.FindSubModule(SceneEditor::kModuleId));
        if (sceneEditor == nullptr)
        {
            return MakeError(request, "InternalError", "SceneEditor module is unavailable.");
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
                sceneEditor->GetPropertyCommandSerializerOptions()))
        {
            return MakeError(
                request,
                "InvalidPropertyPath",
                codecError,
                Json{{"objectId", objectId}, {"propertyPath", normalizedPath}});
        }

        return FromEditorResult(
            request,
            MakeOk(),
            Json{
                {"objectId", objectId},
                {"propertyPath", normalizedPath},
                {"value", std::move(value)}});
    }

    AutomationResponse EditorAutomationSurface::HandleCreateGameObject(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSession(context, request, errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        Scene* scene = nullptr;
        const SceneEditorResult sceneResult = m_SceneServices.GetScene(context, *session, &scene);
        if (!sceneResult.Ok)
        {
            return FromEditorResult(request, sceneResult);
        }

        const std::string name = request.Payload.value("name", "");
        const std::string parentObjectId = request.Payload.value("parentObjectId", "");
        GameObject* parent = nullptr;
        if (!parentObjectId.empty())
        {
            GUID parentGuid;
            if (!ScenePropertyValueCodec::TryParseGuid(parentObjectId, parentGuid))
            {
                return MakeError(request, "InvalidObjectId", "parentObjectId is not a valid GUID.");
            }
            const SceneEditorResult parentResult =
                m_SceneServices.FindGameObject(*scene, parentGuid, &parent);
            if (!parentResult.Ok)
            {
                return FromEditorResult(request, parentResult);
            }
        }

        GameObject* created = nullptr;
        const SceneEditorResult createResult =
            m_SceneServices.CreateGameObject(context, *session, name, parent, &created);
        if (!createResult.Ok)
        {
            return FromEditorResult(request, createResult);
        }

        return FromEditorResult(
            request,
            createResult,
            Json{
                {"objectId", created->GetGuid().ToString()},
                {"name", created->GetName()}});
    }

    AutomationResponse EditorAutomationSurface::HandleDeleteGameObject(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string objectId = request.Payload.value("objectId", "");
        if (objectId.empty())
        {
            return MakeError(request, "InvalidPayload", "objectId is required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSession(context, request, errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        Scene* scene = nullptr;
        const SceneEditorResult sceneResult = m_SceneServices.GetScene(context, *session, &scene);
        if (!sceneResult.Ok)
        {
            return FromEditorResult(request, sceneResult);
        }

        GUID guid;
        if (!ScenePropertyValueCodec::TryParseGuid(objectId, guid))
        {
            return MakeError(request, "InvalidObjectId", "objectId is not a valid GUID.");
        }

        GameObject* gameObject = nullptr;
        const SceneEditorResult findResult = m_SceneServices.FindGameObject(*scene, guid, &gameObject);
        if (!findResult.Ok)
        {
            return FromEditorResult(request, findResult);
        }

        const SceneEditorResult deleteResult =
            m_SceneServices.DeleteGameObject(context, *session, *gameObject);
        if (!deleteResult.Ok)
        {
            return FromEditorResult(request, deleteResult);
        }

        return FromEditorResult(request, deleteResult, Json{{"deletedObjectId", objectId}});
    }

    AutomationResponse EditorAutomationSurface::HandleAddComponent(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string objectId = request.Payload.value("objectId", "");
        const std::string className = request.Payload.value("className", "");
        if (objectId.empty() || className.empty())
        {
            return MakeError(request, "InvalidPayload", "objectId and className are required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSession(context, request, errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        Scene* scene = nullptr;
        const SceneEditorResult sceneResult = m_SceneServices.GetScene(context, *session, &scene);
        if (!sceneResult.Ok)
        {
            return FromEditorResult(request, sceneResult);
        }

        GUID guid;
        if (!ScenePropertyValueCodec::TryParseGuid(objectId, guid))
        {
            return MakeError(request, "InvalidObjectId", "objectId is not a valid GUID.");
        }

        GameObject* gameObject = nullptr;
        const SceneEditorResult findResult = m_SceneServices.FindGameObject(*scene, guid, &gameObject);
        if (!findResult.Ok)
        {
            return FromEditorResult(request, findResult);
        }

        Component* added = nullptr;
        const SceneEditorResult addResult =
            m_SceneServices.AddComponent(context, *session, *gameObject, className, &added);
        if (!addResult.Ok)
        {
            return FromEditorResult(request, addResult);
        }

        return FromEditorResult(
            request,
            addResult,
            Json{
                {"objectId", objectId},
                {"componentObjectId", added->GetGuid().ToString()},
                {"className", added->GetClass() != nullptr ? added->GetClass()->GetName() : className}});
    }

    AutomationResponse EditorAutomationSurface::HandleRemoveComponent(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string componentObjectId = request.Payload.value("componentObjectId", "");
        if (componentObjectId.empty())
        {
            return MakeError(request, "InvalidPayload", "componentObjectId is required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSession(context, request, errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        Scene* scene = nullptr;
        const SceneEditorResult sceneResult = m_SceneServices.GetScene(context, *session, &scene);
        if (!sceneResult.Ok)
        {
            return FromEditorResult(request, sceneResult);
        }

        GUID guid;
        if (!ScenePropertyValueCodec::TryParseGuid(componentObjectId, guid))
        {
            return MakeError(request, "InvalidObjectId", "componentObjectId is not a valid GUID.");
        }

        Component* component = nullptr;
        const SceneEditorResult findResult =
            m_SceneServices.FindComponent(*scene, guid, &component, nullptr);
        if (!findResult.Ok)
        {
            return FromEditorResult(request, findResult);
        }

        const SceneEditorResult removeResult =
            m_SceneServices.RemoveComponent(context, *session, *component);
        if (!removeResult.Ok)
        {
            return FromEditorResult(request, removeResult);
        }

        return FromEditorResult(
            request,
            removeResult,
            Json{{"componentObjectId", componentObjectId}});
    }

    AutomationResponse EditorAutomationSurface::HandleSetProperty(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string objectId = request.Payload.value("objectId", "");
        const std::string propertyPath = request.Payload.value("propertyPath", "");
        if (objectId.empty() || propertyPath.empty() || !request.Payload.contains("value"))
        {
            return MakeError(request, "InvalidPayload", "objectId, propertyPath, and value are required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSession(context, request, errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        GUID guid;
        if (!ScenePropertyValueCodec::TryParseGuid(objectId, guid))
        {
            return MakeError(request, "InvalidObjectId", "objectId is not a valid GUID.");
        }

        std::shared_ptr<MEObject> owner = ObjectManager::Get().FindObject(guid);
        if (!owner)
        {
            return MakeError(request, "InvalidObjectId", "No object with this GUID.");
        }

        const Reflection::MEClass* ownerClass = owner->GetClass();
        if (ownerClass == nullptr)
        {
            return MakeError(request, "InternalError", "Owner class is null.");
        }

        SceneEditor* sceneEditor = dynamic_cast<SceneEditor*>(context.FindSubModule(SceneEditor::kModuleId));
        if (sceneEditor == nullptr)
        {
            return MakeError(request, "InternalError", "SceneEditor module is unavailable.");
        }

        const std::string normalizedPath = ScenePropertyValueCodec::NormalizePropertyPath(propertyPath);
        const Serialization::SerializerOptions options = sceneEditor->GetPropertyCommandSerializerOptions();
        const Json& value = request.Payload.at("value");

        std::vector<uint8_t> beforeBlob;
        std::string codecError;
        if (!ScenePropertyValueCodec::TryCaptureBlob(
                *owner, *ownerClass, normalizedPath, beforeBlob, codecError, options))
        {
            return MakeError(request, "InvalidPropertyPath", codecError);
        }

        if (!ScenePropertyValueCodec::TryWriteJsonToLive(
                *owner, *ownerClass, normalizedPath, value, codecError, options))
        {
            ScenePropertyValueCodec::TryRestoreBlob(
                *owner, *ownerClass, normalizedPath, beforeBlob, codecError, options);
            return MakeError(
                request,
                "InvalidPayload",
                codecError,
                Json{{"objectId", objectId}, {"propertyPath", normalizedPath}});
        }

        void* leafOwner = nullptr;
        const Reflection::MEClass* leafOwnerClass = nullptr;
        std::string leafPropertyName;
        std::string walkError;
        if (!ScenePropertyValueCodec::WalkToLeafOwner(
                owner.get(),
                ownerClass,
                normalizedPath,
                leafOwner,
                leafOwnerClass,
                leafPropertyName,
                walkError))
        {
            ScenePropertyValueCodec::TryRestoreBlob(
                *owner, *ownerClass, normalizedPath, beforeBlob, codecError, options);
            return MakeError(request, "InvalidPropertyPath", walkError);
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
            ScenePropertyValueCodec::TryRestoreBlob(
                *owner, *ownerClass, normalizedPath, beforeBlob, codecError, options);
            return MakeError(request, "InvalidPropertyPath", "Leaf property not found.");
        }

        const size_t storageSize = property->GetStorageSize();
        std::vector<uint8_t> afterBuffer(storageSize);
        const SceneEditorResult afterGet = m_SceneServices.GetProperty(
            *owner, normalizedPath, afterBuffer.data(), afterBuffer.size());
        if (!afterGet.Ok)
        {
            ScenePropertyValueCodec::TryRestoreBlob(
                *owner, *ownerClass, normalizedPath, beforeBlob, codecError, options);
            return FromEditorResult(request, afterGet);
        }

        if (!ScenePropertyValueCodec::TryRestoreBlob(
                *owner, *ownerClass, normalizedPath, beforeBlob, codecError, options))
        {
            return MakeError(request, "InternalError", "Failed to restore pre-edit value: " + codecError);
        }

        std::vector<uint8_t> beforeBuffer(storageSize);
        const SceneEditorResult beforeGet = m_SceneServices.GetProperty(
            *owner, normalizedPath, beforeBuffer.data(), beforeBuffer.size());
        if (!beforeGet.Ok)
        {
            return FromEditorResult(request, beforeGet);
        }

        if (beforeBuffer == afterBuffer)
        {
            return FromEditorResult(
                request,
                MakeOk(),
                Json{
                    {"objectId", objectId},
                    {"propertyPath", normalizedPath},
                    {"value", value},
                    {"unchanged", true}});
        }

        const SceneEditorResult setResult = m_SceneServices.SetProperty(
            context, *session, *owner, normalizedPath, afterBuffer.data(), afterBuffer.size());
        if (!setResult.Ok)
        {
            return FromEditorResult(request, setResult);
        }

        return FromEditorResult(
            request,
            setResult,
            Json{
                {"objectId", objectId},
                {"propertyPath", normalizedPath},
                {"value", value}});
    }

    AutomationResponse EditorAutomationSurface::HandleReparent(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string objectId = request.Payload.value("objectId", "");
        if (objectId.empty())
        {
            return MakeError(request, "InvalidPayload", "objectId is required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSession(context, request, errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        Scene* scene = nullptr;
        const SceneEditorResult sceneResult = m_SceneServices.GetScene(context, *session, &scene);
        if (!sceneResult.Ok)
        {
            return FromEditorResult(request, sceneResult);
        }

        GUID guid;
        if (!ScenePropertyValueCodec::TryParseGuid(objectId, guid))
        {
            return MakeError(request, "InvalidObjectId", "objectId is not a valid GUID.");
        }

        GameObject* gameObject = nullptr;
        const SceneEditorResult findResult = m_SceneServices.FindGameObject(*scene, guid, &gameObject);
        if (!findResult.Ok)
        {
            return FromEditorResult(request, findResult);
        }

        GameObject* newParent = nullptr;
        const std::string newParentObjectId = request.Payload.value("newParentObjectId", "");
        if (!newParentObjectId.empty())
        {
            GUID parentGuid;
            if (!ScenePropertyValueCodec::TryParseGuid(newParentObjectId, parentGuid))
            {
                return MakeError(request, "InvalidObjectId", "newParentObjectId is not a valid GUID.");
            }
            const SceneEditorResult parentResult =
                m_SceneServices.FindGameObject(*scene, parentGuid, &newParent);
            if (!parentResult.Ok)
            {
                return FromEditorResult(request, parentResult);
            }
        }

        const SceneEditorResult reparentResult =
            m_SceneServices.Reparent(context, *session, *gameObject, newParent);
        if (!reparentResult.Ok)
        {
            return FromEditorResult(request, reparentResult);
        }

        return FromEditorResult(
            request,
            reparentResult,
            Json{
                {"objectId", objectId},
                {"newParentObjectId", newParentObjectId}});
    }

    AutomationResponse EditorAutomationSurface::HandleSave(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSession(context, request, errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        const SceneEditorResult saveResult = m_SceneServices.SaveDocument(context, *session);
        if (!saveResult.Ok)
        {
            return FromEditorResult(request, saveResult);
        }
        return FromEditorResult(request, saveResult, Json{{"saved", true}});
    }

    AutomationResponse EditorAutomationSurface::HandleValidate(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSession(context, request, errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        const std::string objectId = request.Payload.value("objectId", "");
        const std::string propertyPath = request.Payload.value("propertyPath", "");
        MEObject* object = nullptr;
        if (!objectId.empty())
        {
            GUID guid;
            if (ScenePropertyValueCodec::TryParseGuid(objectId, guid))
            {
                std::shared_ptr<MEObject> owner = ObjectManager::Get().FindObject(guid);
                object = owner.get();
            }
        }

        const SceneEditorResult validateResult =
            m_SceneServices.Validate(context, *session, object, propertyPath);
        if (!validateResult.Ok)
        {
            return FromEditorResult(request, validateResult);
        }
        return FromEditorResult(
            request,
            validateResult,
            Json{
                {"valid", true},
                {"stub", true},
                {"message", "Validation is a P0 stub."}});
    }

    bool EditorAutomationSurface::TryReadStringArray(
        const Json& payload,
        const char* key,
        std::vector<std::string>& outNames,
        std::string& outError)
    {
        outNames.clear();
        if (!payload.contains(key) || payload[key].is_null())
        {
            return true;
        }
        if (!payload[key].is_array())
        {
            outError = std::string(key) + " must be an array of strings.";
            return false;
        }
        for (const Json& item : payload[key])
        {
            if (!item.is_string())
            {
                outError = std::string(key) + " must contain only strings.";
                return false;
            }
            outNames.push_back(item.get<std::string>());
        }
        return true;
    }

    AutomationResponse EditorAutomationSurface::HandleMoveComponent(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string componentObjectId = request.Payload.value("componentObjectId", "");
        if (componentObjectId.empty() || !request.Payload.contains("newIndex") || !request.Payload["newIndex"].is_number())
        {
            return MakeError(request, "InvalidPayload", "componentObjectId and numeric newIndex are required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSession(context, request, errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        Scene* scene = nullptr;
        const SceneEditorResult sceneResult = m_SceneServices.GetScene(context, *session, &scene);
        if (!sceneResult.Ok || scene == nullptr)
        {
            return FromEditorResult(request, sceneResult);
        }

        GUID componentGuid;
        if (!ScenePropertyValueCodec::TryParseGuid(componentObjectId, componentGuid))
        {
            return MakeError(request, "InvalidObjectId", "componentObjectId is not a valid GUID.");
        }

        Component* component = nullptr;
        GameObject* owner = nullptr;
        const SceneEditorResult findResult =
            m_SceneServices.FindComponent(*scene, componentGuid, &component, &owner);
        if (!findResult.Ok || component == nullptr || owner == nullptr)
        {
            return FromEditorResult(request, findResult);
        }

        const size_t newIndex = request.Payload["newIndex"].get<size_t>();
        const SceneEditorResult moveResult =
            m_SceneServices.MoveComponent(context, *session, *owner, *component, newIndex);
        return FromEditorResult(request, moveResult, Json{{"moved", moveResult.Ok}});
    }

    AutomationResponse EditorAutomationSurface::HandleInstantiatePrefab(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string assetPath = request.Payload.value("assetPath", "");
        if (assetPath.empty())
        {
            return MakeError(request, "InvalidPayload", "assetPath is required.");
        }
        if (!AssetManager::HasInstance())
        {
            return MakeError(request, "InternalError", "AssetManager is not available.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSession(context, request, errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        std::shared_ptr<Prefab> prefab = ([&]() -> std::shared_ptr<Prefab> { std::string _loadAssetError; return std::dynamic_pointer_cast<Prefab>(AssetManager::Get().LoadAssetByPath(assetPath, _loadAssetError)); })();
        if (!prefab)
        {
            return MakeError(request, "InvalidPayload", "Failed to load prefab.");
        }

        GameObject* attachParent = nullptr;
        const std::string parentId = request.Payload.value("attachParentObjectId", "");
        if (!parentId.empty())
        {
            Scene* scene = nullptr;
            const SceneEditorResult sceneResult = m_SceneServices.GetScene(context, *session, &scene);
            if (!sceneResult.Ok || scene == nullptr)
            {
                return FromEditorResult(request, sceneResult);
            }
            GUID parentGuid;
            if (!ScenePropertyValueCodec::TryParseGuid(parentId, parentGuid))
            {
                return MakeError(request, "InvalidObjectId", "attachParentObjectId is not a valid GUID.");
            }
            const SceneEditorResult findResult = m_SceneServices.FindGameObject(*scene, parentGuid, &attachParent);
            if (!findResult.Ok)
            {
                return FromEditorResult(request, findResult);
            }
        }

        const SceneEditorResult instantiateResult =
            m_SceneServices.InstantiatePrefab(context, *session, std::move(prefab), attachParent);
        return FromEditorResult(request, instantiateResult, Json{{"instantiated", instantiateResult.Ok}});
    }

    AutomationResponse EditorAutomationSurface::HandleSelectObject(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string objectId = request.Payload.value("objectId", "");
        if (objectId.empty())
        {
            return MakeError(request, "InvalidPayload", "objectId is required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSession(context, request, errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        GUID guid;
        if (!ScenePropertyValueCodec::TryParseGuid(objectId, guid))
        {
            return MakeError(request, "InvalidObjectId", "objectId is not a valid GUID.");
        }

        const EditorControlResult selectResult =
            context.GetEditorControlServices().SelectObject(context, *session, guid);
        if (!selectResult.Ok)
        {
            return MakeError(request, "InvalidPayload", selectResult.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"sessionId", selectResult.SessionId}, {"objectId", objectId}});
    }

    AutomationResponse EditorAutomationSurface::HandleListClasses(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        std::vector<std::string> requireNames;
        std::vector<std::string> excludeNames;
        std::string parseError;
        if (!TryReadStringArray(request.Payload, "requireAll", requireNames, parseError)
            || !TryReadStringArray(request.Payload, "excludeAny", excludeNames, parseError))
        {
            return MakeError(request, "InvalidPayload", parseError);
        }

        TypeQueryServices& typeQuery = context.GetTypeQueryServices();
        Reflection::ClassSpecifierMask requireAll = 0;
        Reflection::ClassSpecifierMask excludeAny = 0;
        if (!typeQuery.TryParseClassSpecifierMask(requireNames, requireAll, parseError)
            || !typeQuery.TryParseClassSpecifierMask(excludeNames, excludeAny, parseError))
        {
            return MakeError(request, "InvalidPayload", parseError);
        }

        const Reflection::MEClass* base = nullptr;
        const std::string baseClassName = request.Payload.value("baseClass", "");
        if (!baseClassName.empty())
        {
            base = Reflection::ReflectionSystem::Get().FindClass(baseClassName);
            if (base == nullptr)
            {
                return MakeError(request, "InvalidPayload", "baseClass was not found.");
            }
        }

        Json classes = Json::array();
        for (const Reflection::MEClass* classInfo : typeQuery.QueryClasses(base, requireAll, excludeAny))
        {
            if (classInfo == nullptr)
            {
                continue;
            }
            std::vector<std::string> specifierNames;
            typeQuery.AppendClassSpecifierNames(classInfo->GetSpecifierMask(), specifierNames);
            const Reflection::MEClass* superClass = classInfo->GetSuperClass();
            classes.push_back(Json{
                {"name", classInfo->GetName()},
                {"super", superClass != nullptr ? superClass->GetName() : std::string{}},
                {"specifiers", specifierNames}});
        }
        return FromEditorResult(request, MakeOk(), Json{{"classes", std::move(classes)}});
    }

    AutomationResponse EditorAutomationSurface::HandleListProperties(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string className = request.Payload.value("className", "");
        if (className.empty())
        {
            return MakeError(request, "InvalidPayload", "className is required.");
        }

        const Reflection::MEClass* owner = Reflection::ReflectionSystem::Get().FindClass(className);
        if (owner == nullptr)
        {
            return MakeError(request, "InvalidPayload", "className was not found.");
        }

        std::vector<std::string> requireNames;
        std::vector<std::string> excludeNames;
        std::string parseError;
        if (!TryReadStringArray(request.Payload, "requireAll", requireNames, parseError)
            || !TryReadStringArray(request.Payload, "excludeAny", excludeNames, parseError))
        {
            return MakeError(request, "InvalidPayload", parseError);
        }

        TypeQueryServices& typeQuery = context.GetTypeQueryServices();
        Reflection::PropertySpecifierMask requireAll = 0;
        Reflection::PropertySpecifierMask excludeAny = 0;
        if (!typeQuery.TryParsePropertySpecifierMask(requireNames, requireAll, parseError)
            || !typeQuery.TryParsePropertySpecifierMask(excludeNames, excludeAny, parseError))
        {
            return MakeError(request, "InvalidPayload", parseError);
        }

        Json properties = Json::array();
        for (const Reflection::MEProperty* property : typeQuery.QueryProperties(*owner, requireAll, excludeAny))
        {
            if (property == nullptr)
            {
                continue;
            }
            std::vector<std::string> specifierNames;
            typeQuery.AppendPropertySpecifierNames(property->GetSpecifierMask(), specifierNames);
            properties.push_back(Json{
                {"name", property->GetName()},
                {"type", TypeQueryServices::DescribePropertyType(*property)},
                {"specifiers", specifierNames}});
        }
        return FromEditorResult(request, MakeOk(), Json{{"properties", std::move(properties)}});
    }

    AutomationResponse EditorAutomationSurface::HandleListAssets(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        std::vector<const AssetMeta*> metas;
        const AssetEditorResult listResult = context.GetAssetEditorServices().List(
            request.Payload.value("directory", ""),
            request.Payload.value("assetType", ""),
            metas);
        if (!listResult.Ok)
        {
            return MakeError(request, "InternalError", listResult.Message);
        }

        Json assets = Json::array();
        for (const AssetMeta* meta : metas)
        {
            if (meta == nullptr)
            {
                continue;
            }
            assets.push_back(Json{
                {"path", meta->AssetPath},
                {"guid", meta->Guid.ToString()},
                {"assetType", meta->AssetType},
                {"name", meta->AssetName}});
        }
        return FromEditorResult(request, MakeOk(), Json{{"assets", std::move(assets)}});
    }

    AutomationResponse EditorAutomationSurface::HandleCreateAsset(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        std::string path;
        std::string guid;
        const AssetEditorResult createResult = context.GetAssetEditorServices().Create(
            request.Payload.value("assetType", ""),
            request.Payload.value("directory", ""),
            request.Payload.value("name", ""),
            path,
            guid);
        if (!createResult.Ok)
        {
            return MakeError(request, "InvalidPayload", createResult.Message);
        }
        return FromEditorResult(
            request,
            MakeOk(),
            Json{{"path", path}, {"guid", guid}});
    }

    AutomationResponse EditorAutomationSurface::HandleDeleteAsset(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const AssetEditorResult deleteResult =
            context.GetAssetEditorServices().Delete(context, request.Payload.value("path", ""));
        if (!deleteResult.Ok)
        {
            return MakeError(request, "InvalidPayload", deleteResult.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"deleted", true}});
    }

    AutomationResponse EditorAutomationSurface::HandleOpenAsset(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const EditorControlResult openResult =
            context.GetEditorControlServices().OpenAsset(context, request.Payload.value("path", ""));
        if (!openResult.Ok)
        {
            return MakeError(request, "InvalidPayload", openResult.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"sessionId", openResult.SessionId}});
    }

    AutomationResponse EditorAutomationSurface::HandleActivateSession(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const EditorControlResult activateResult =
            context.GetEditorControlServices().ActivateSession(context, request.SessionId);
        if (!activateResult.Ok)
        {
            return MakeError(request, "InvalidPayload", activateResult.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"sessionId", activateResult.SessionId}});
    }

    AutomationResponse EditorAutomationSurface::HandleRevealAsset(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const EditorControlResult revealResult =
            context.GetEditorControlServices().RevealAsset(context, request.Payload.value("path", ""));
        if (!revealResult.Ok)
        {
            return MakeError(request, "InvalidPayload", revealResult.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"revealed", true}});
    }
    AutomationResponse EditorAutomationSurface::HandleListImportProducts(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        std::vector<AssetImportProductInfo> products;
        const std::string source = request.Payload.value("sourcePath", request.Payload.value("extension", ""));
        const AssetEditorResult result = context.GetAssetEditorServices().ListImportProducts(source, products);
        if (!result.Ok)
        {
            return MakeError(request, "InvalidPayload", result.Message);
        }

        Json productsJson = Json::array();
        for (const AssetImportProductInfo& product : products)
        {
            Json required = Json::array();
            for (const AssetImportOptionDescriptor& option : product.RequiredOptions)
            {
                required.push_back(Json{
                    {"key", option.Key},
                    {"type", option.Type},
                    {"meaning", option.Meaning}});
            }
            Json optional = Json::array();
            for (const AssetImportOptionDescriptor& option : product.OptionalOptions)
            {
                optional.push_back(Json{
                    {"key", option.Key},
                    {"type", option.Type},
                    {"meaning", option.Meaning}});
            }
            productsJson.push_back(Json{
                {"productId", product.ProductId},
                {"displayName", product.DisplayName},
                {"requiredOptions", std::move(required)},
                {"optionalOptions", std::move(optional)},
                {"notes", product.Notes}});
        }
        return FromEditorResult(request, MakeOk(), Json{{"products", std::move(productsJson)}});
    }

    AutomationResponse EditorAutomationSurface::HandleImportAsset(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        std::vector<AssetImportProductSpec> products;
        if (request.Payload.contains("products") && request.Payload["products"].is_array())
        {
            for (const Json& item : request.Payload["products"])
            {
                AssetImportProductSpec spec;
                spec.ProductId = item.value("productId", "");
                if (item.contains("options") && item["options"].is_object())
                {
                    for (auto it = item["options"].begin(); it != item["options"].end(); ++it)
                    {
                        if (it.value().is_string())
                        {
                            spec.Options[it.key()] = it.value().get<std::string>();
                        }
                        else
                        {
                            spec.Options[it.key()] = it.value().dump();
                        }
                    }
                }
                products.push_back(std::move(spec));
            }
        }
        else if (request.Payload.contains("productId"))
        {
            AssetImportProductSpec spec;
            spec.ProductId = request.Payload.value("productId", "");
            products.push_back(std::move(spec));
        }

        std::vector<AssetImportCreatedInfo> created;
        std::vector<std::string> warnings;
        const AssetEditorResult result = context.GetAssetEditorServices().Import(
            request.Payload.value("sourcePath", ""),
            request.Payload.value("destDirectory", ""),
            products,
            request.Payload.value("overwrite", false),
            created,
            warnings);
        if (!result.Ok)
        {
            return MakeError(request, "InvalidPayload", result.Message);
        }

        Json createdJson = Json::array();
        for (const AssetImportCreatedInfo& info : created)
        {
            createdJson.push_back(Json{
                {"path", info.AssetPath},
                {"assetType", info.AssetTypeId},
                {"guid", info.Guid}});
        }
        return FromEditorResult(
            request,
            MakeOk(),
            Json{{"created", std::move(createdJson)}, {"warnings", warnings}});
    }

    AutomationResponse EditorAutomationSurface::HandleCreateFolder(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        std::string created;
        const AssetEditorResult result = context.GetAssetEditorServices().CreateFolder(
            request.Payload.value("parentDirectory", ""),
            request.Payload.value("name", ""),
            created);
        if (!result.Ok)
        {
            return MakeError(request, "InvalidPayload", result.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"path", created}});
    }

    AutomationResponse EditorAutomationSurface::HandleMoveAsset(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const AssetEditorResult result = context.GetAssetEditorServices().Move(
            request.Payload.value("oldPath", ""),
            request.Payload.value("newPath", ""));
        if (!result.Ok)
        {
            return MakeError(request, "InvalidPayload", result.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"moved", true}});
    }

    AutomationResponse EditorAutomationSurface::HandleRenameAssetOp(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const AssetEditorResult result = context.GetAssetEditorServices().Rename(
            request.Payload.value("path", ""),
            request.Payload.value("newFileName", ""));
        if (!result.Ok)
        {
            return MakeError(request, "InvalidPayload", result.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"renamed", true}});
    }

    AutomationResponse EditorAutomationSurface::HandleRenameFolder(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const AssetEditorResult result = context.GetAssetEditorServices().RenameFolder(
            request.Payload.value("path", request.Payload.value("directory", "")),
            request.Payload.value("newName", ""));
        if (!result.Ok)
        {
            return MakeError(request, "InvalidPayload", result.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"renamed", true}});
    }

    AutomationResponse EditorAutomationSurface::HandleDeleteFolder(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const AssetEditorResult result = context.GetAssetEditorServices().DeleteFolder(
            request.Payload.value("path", request.Payload.value("directory", "")));
        if (!result.Ok)
        {
            return MakeError(request, "InvalidPayload", result.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"deleted", true}});
    }

    AutomationResponse EditorAutomationSurface::HandleBatchRename(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        std::vector<std::string> paths;
        if (request.Payload.contains("paths") && request.Payload["paths"].is_array())
        {
            for (const Json& item : request.Payload["paths"])
            {
                if (item.is_string())
                {
                    paths.push_back(item.get<std::string>());
                }
            }
        }
        const AssetEditorResult result = context.GetAssetEditorServices().BatchRename(
            paths,
            request.Payload.value("baseStem", ""));
        if (!result.Ok)
        {
            return MakeError(request, "InvalidPayload", result.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"renamed", true}, {"count", paths.size()}});
    }

    AutomationResponse EditorAutomationSurface::HandleSpriteCreateFromTexture(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string texturePath = request.Payload.value("texturePath", "");
        std::string loadError;
        std::shared_ptr<Texture2D> texture = ([&]() -> std::shared_ptr<Texture2D> { std::string _loadAssetError; return std::dynamic_pointer_cast<Texture2D>(AssetManager::Get().LoadAssetByPath(texturePath, _loadAssetError)); })();
        if (texture == nullptr)
        {
            return MakeError(request, "InvalidPayload", "Failed to load Texture2D: " + texturePath);
        }

        Vector4 rect(0.0f, 0.0f, 0.0f, 0.0f);
        if (request.Payload.contains("sourceRect") && request.Payload["sourceRect"].is_array()
            && request.Payload["sourceRect"].size() >= 4)
        {
            rect.x = request.Payload["sourceRect"][0].get<float>();
            rect.y = request.Payload["sourceRect"][1].get<float>();
            rect.z = request.Payload["sourceRect"][2].get<float>();
            rect.w = request.Payload["sourceRect"][3].get<float>();
        }

        std::shared_ptr<Sprite> sprite;
        std::string outPath;
        std::string outGuid;
        const GraphEditorResult result = context.GetSpriteEditorServices().CreateSpriteFromTexture(
            context.GetAssetEditorServices(),
            texture,
            rect,
            request.Payload.value("directory", ""),
            request.Payload.value("name", ""),
            &sprite,
            &outPath,
            &outGuid);
        if (!result.Ok)
        {
            return MakeError(request, "InternalError", result.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"path", outPath}, {"guid", outGuid}});
    }

    AutomationResponse EditorAutomationSurface::HandleSpriteSave(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string path = request.Payload.value("path", "");
        std::shared_ptr<Sprite> sprite = ([&]() -> std::shared_ptr<Sprite> { std::string _loadAssetError; return std::dynamic_pointer_cast<Sprite>(AssetManager::Get().LoadAssetByPath(path, _loadAssetError)); })();
        if (sprite == nullptr)
        {
            return MakeError(request, "InvalidPayload", "Failed to load Sprite: " + path);
        }
        const GraphEditorResult result = context.GetSpriteEditorServices().SaveSprite(*sprite);
        if (!result.Ok)
        {
            return MakeError(request, "InternalError", result.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"saved", true}});
    }

    AutomationResponse EditorAutomationSurface::HandleSpriteAtlasCreate(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        std::shared_ptr<SpriteAtlas> atlas;
        std::string outPath;
        std::string outGuid;
        const GraphEditorResult result = context.GetSpriteEditorServices().CreateAtlas(
            context.GetAssetEditorServices(),
            request.Payload.value("directory", ""),
            request.Payload.value("name", ""),
            &atlas,
            &outPath,
            &outGuid);
        if (!result.Ok)
        {
            return MakeError(request, "InternalError", result.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"path", outPath}, {"guid", outGuid}});
    }

    AutomationResponse EditorAutomationSurface::HandleSpriteAtlasAddMembers(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string atlasPath = request.Payload.value("atlasPath", "");
        std::shared_ptr<SpriteAtlas> atlas = ([&]() -> std::shared_ptr<SpriteAtlas> { std::string _loadAssetError; return std::dynamic_pointer_cast<SpriteAtlas>(AssetManager::Get().LoadAssetByPath(atlasPath, _loadAssetError)); })();
        if (atlas == nullptr)
        {
            return MakeError(request, "InvalidPayload", "Failed to load SpriteAtlas: " + atlasPath);
        }

        std::vector<std::shared_ptr<Sprite>> sprites;
        if (request.Payload.contains("spritePaths") && request.Payload["spritePaths"].is_array())
        {
            for (const Json& item : request.Payload["spritePaths"])
            {
                if (!item.is_string())
                {
                    continue;
                }
                std::shared_ptr<Sprite> sprite = ([&]() -> std::shared_ptr<Sprite> { std::string _loadAssetError; return std::dynamic_pointer_cast<Sprite>(AssetManager::Get().LoadAssetByPath(item.get<std::string>(), _loadAssetError)); })();
                if (sprite != nullptr)
                {
                    sprites.push_back(sprite);
                }
            }
        }

        const GraphEditorResult result = context.GetSpriteEditorServices().AddAtlasMembers(
            context.GetCommandStack(),
            *atlas,
            sprites);
        if (!result.Ok)
        {
            return MakeError(request, "InternalError", result.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"added", sprites.size()}});
    }

    AutomationResponse EditorAutomationSurface::HandleSpriteAtlasSetPadding(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string atlasPath = request.Payload.value("atlasPath", request.Payload.value("path", ""));
        std::shared_ptr<SpriteAtlas> atlas = ([&]() -> std::shared_ptr<SpriteAtlas> { std::string _loadAssetError; return std::dynamic_pointer_cast<SpriteAtlas>(AssetManager::Get().LoadAssetByPath(atlasPath, _loadAssetError)); })();
        if (atlas == nullptr)
        {
            return MakeError(request, "InvalidPayload", "Failed to load SpriteAtlas: " + atlasPath);
        }

        const int32_t padding = request.Payload.value("padding", atlas->GetPadding());
        SpriteAtlasPaddingMode mode = atlas->GetPaddingMode();
        const std::string modeStr = request.Payload.value("paddingMode", "");
        if (modeStr == "DilateBorder" || modeStr == "dilate")
        {
            mode = SpriteAtlasPaddingMode::DilateBorder;
        }
        else if (modeStr == "PadWithZero" || modeStr == "zero" || !modeStr.empty())
        {
            if (modeStr == "PadWithZero" || modeStr == "zero")
            {
                mode = SpriteAtlasPaddingMode::PadWithZero;
            }
        }

        const GraphEditorResult result =
            context.GetSpriteEditorServices().SetAtlasPadding(*atlas, padding, mode);
        if (!result.Ok)
        {
            return MakeError(request, "InternalError", result.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"padding", padding}});
    }

    AutomationResponse EditorAutomationSurface::HandleSpriteAtlasRebuild(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        (void)context;
        const std::string atlasPath = request.Payload.value("atlasPath", request.Payload.value("path", ""));
        std::shared_ptr<SpriteAtlas> atlas = ([&]() -> std::shared_ptr<SpriteAtlas> { std::string _loadAssetError; return std::dynamic_pointer_cast<SpriteAtlas>(AssetManager::Get().LoadAssetByPath(atlasPath, _loadAssetError)); })();
        if (atlas == nullptr)
        {
            return MakeError(request, "InvalidPayload", "Failed to load SpriteAtlas: " + atlasPath);
        }
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (rhi == nullptr)
        {
            return MakeError(request, "InternalError", "RHI is not available.");
        }
        const GraphEditorResult result = context.GetSpriteEditorServices().RebuildAtlas(*atlas, *rhi);
        if (!result.Ok)
        {
            return MakeError(request, "InternalError", result.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"rebuilt", true}});
    }

    AutomationResponse EditorAutomationSurface::HandleSpriteAtlasSave(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string atlasPath = request.Payload.value("atlasPath", request.Payload.value("path", ""));
        std::shared_ptr<SpriteAtlas> atlas = ([&]() -> std::shared_ptr<SpriteAtlas> { std::string _loadAssetError; return std::dynamic_pointer_cast<SpriteAtlas>(AssetManager::Get().LoadAssetByPath(atlasPath, _loadAssetError)); })();
        if (atlas == nullptr)
        {
            return MakeError(request, "InvalidPayload", "Failed to load SpriteAtlas: " + atlasPath);
        }
        const GraphEditorResult result = context.GetSpriteEditorServices().SaveAtlas(*atlas);
        if (!result.Ok)
        {
            return MakeError(request, "InternalError", result.Message);
        }
        return FromEditorResult(request, MakeOk(), Json{{"saved", true}});
    }

    AutomationResponse EditorAutomationSurface::HandleSpriteAtlasCreateFromSelection(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        std::vector<const AssetMeta*> selected;
        if (request.Payload.contains("spritePaths") && request.Payload["spritePaths"].is_array())
        {
            for (const Json& item : request.Payload["spritePaths"])
            {
                if (!item.is_string())
                {
                    continue;
                }
                if (const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(item.get<std::string>()))
                {
                    selected.push_back(meta);
                }
            }
        }
        if (!context.GetAssetWorkflow().TryCreateSpriteAtlasFromSelection(selected))
        {
            return MakeError(request, "InternalError", "Create atlas from selection failed.");
        }
        return FromEditorResult(request, MakeOk(), Json{{"created", true}});
    }

}
