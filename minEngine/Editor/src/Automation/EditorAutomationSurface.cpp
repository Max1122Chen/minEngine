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
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"
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
#include "SubEditor/Prefab/PrefabEditConstraints.h"
#include "SubEditor/Prefab/PrefabStage.h"
#include "SubEditor/Prefab/PrefabStageController.h"
#include "Services/Sprite/SpriteEditorServices.h"
#include "Services/Control/EditorControlServices.h"
#include "Services/TypeQuery/TypeQueryServices.h"
#include "Shell/Document/EditorDocumentHost.h"
#include "Shell/Document/EditorDocumentSession.h"
#include "Shell/EditorCommandStack.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Scene/SceneEditor.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_set>
#include <vector>

namespace minEngine
{
    namespace
    {
        constexpr const char* kPrefabAssetExtension = ".meprefab";

        SceneEditorResult MakeOk()
        {
            SceneEditorResult result;
            result.Ok = true;
            result.Code = SceneEditorError::None;
            return result;
        }

        bool TryReadWholeFile(const std::filesystem::path& path, std::vector<char>& outBytes)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                return false;
            }

            outBytes.assign(
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());
            return true;
        }

        bool TryWriteWholeFile(const std::filesystem::path& path, const std::vector<char>& bytes)
        {
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                return false;
            }

            stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            return stream.good();
        }

        /**
         * Keeps the bytes an asset file had before a save, so a failed save leaves the previous
         * content on disk instead of a truncated or half-written file.
         */
        struct AssetFileRollback
        {
            std::filesystem::path Path;
            std::vector<char> PreviousBytes;
            bool bHadFile = false;

            void Capture(const std::filesystem::path& path)
            {
                Path = path;
                bHadFile = TryReadWholeFile(path, PreviousBytes);
            }

            void Restore() const
            {
                if (Path.empty())
                {
                    return;
                }

                if (bHadFile)
                {
                    TryWriteWholeFile(Path, PreviousBytes);
                    return;
                }

                std::error_code removeError;
                std::filesystem::remove(Path, removeError);
            }
        };

        /** First non-temporary top-level GameObject of a Prefab Stage scene. */
        std::string FindStageRootGuid(const Scene& stageScene)
        {
            for (const std::shared_ptr<GameObject>& gameObject : stageScene.GetAllGameObjects())
            {
                if (gameObject == nullptr || gameObject->GetParent() != nullptr)
                {
                    continue;
                }
                if (PrefabUtility::IsEditorTempStageObject(*gameObject))
                {
                    continue;
                }
                return gameObject->GetGuid().ToString();
            }
            return std::string();
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
            "sprite_atlas.set_sampler",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleSpriteAtlasSetSampler(context, request);
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
            "prefab.create_from_game_object",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandleCreatePrefabFromGameObject(context, request);
            });
        m_Handlers.emplace(
            "prefab.open",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandlePrefabOpen(context, request);
            });
        m_Handlers.emplace(
            "prefab.save",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandlePrefabSave(context, request);
            });
        m_Handlers.emplace(
            "prefab.close",
            [this](IEditorContext& context, const AutomationRequest& request) {
                return HandlePrefabClose(context, request);
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

        const std::string& sessionTypeId = session->GetTypeId();
        if (sessionTypeId != "Scene" && sessionTypeId != "Prefab")
        {
            outErrorResponse = MakeError(request, "SessionNotFound", "Session is not a Scene or Prefab document.");
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

    const char* EditorAutomationSurface::MapRefErrorCode(ScenePropertyValueCodec::RefError kind)
    {
        switch (kind)
        {
        case ScenePropertyValueCodec::RefError::Unsupported:
            return "Unsupported";
        case ScenePropertyValueCodec::RefError::Unresolved:
            return "UnresolvedObjectRef";
        case ScenePropertyValueCodec::RefError::Invalid:
        case ScenePropertyValueCodec::RefError::None:
        default:
            return "InvalidPayload";
        }
    }

    EditorAutomationSurface::PropertyWriteOutcome EditorAutomationSurface::WritePropertyValue(
        IEditorContext& context,
        EditorDocumentSession& session,
        SceneEditor& sceneEditor,
        MEObject& owner,
        const Reflection::MEClass& ownerClass,
        const std::string& normalizedPath,
        const Json& value,
        const std::vector<uint8_t>& beforeBlob)
    {
        PropertyWriteOutcome outcome;
        const Serialization::SerializerOptions options = sceneEditor.GetPropertyCommandSerializerOptions();

        // Same pipeline as Material/AnimGraph set_property: JSON → live → capture after blob →
        // restore before → commit blobs. Do not snapshot std::string (or other non-trivial
        // properties) through raw GetProperty into an unconstructed byte buffer.
        std::string codecError;
        ScenePropertyValueCodec::RefError refError = ScenePropertyValueCodec::RefError::None;
        if (!ScenePropertyValueCodec::TryWriteJsonToLive(
                owner, ownerClass, normalizedPath, value, codecError, options, &refError))
        {
            std::string restoreError;
            ScenePropertyValueCodec::TryRestoreBlob(
                owner, ownerClass, normalizedPath, beforeBlob, restoreError, options);
            outcome.ErrorCode = MapRefErrorCode(refError);
            outcome.ErrorMessage = codecError;
            return outcome;
        }

        std::vector<uint8_t> afterBlob;
        if (!ScenePropertyValueCodec::TryCaptureBlob(
                owner, ownerClass, normalizedPath, afterBlob, codecError, options))
        {
            std::string restoreError;
            ScenePropertyValueCodec::TryRestoreBlob(
                owner, ownerClass, normalizedPath, beforeBlob, restoreError, options);
            outcome.ErrorCode = "InternalError";
            outcome.ErrorMessage = codecError;
            return outcome;
        }

        if (!ScenePropertyValueCodec::TryRestoreBlob(
                owner, ownerClass, normalizedPath, beforeBlob, codecError, options))
        {
            outcome.ErrorCode = "InternalError";
            outcome.ErrorMessage = "Failed to restore pre-edit value: " + codecError;
            return outcome;
        }

        if (beforeBlob == afterBlob)
        {
            outcome.Ok = true;
            outcome.bUnchanged = true;
            return outcome;
        }

        const SceneEditorResult commitResult = m_SceneServices.CommitObjectPropertyBlobs(
            context,
            session,
            owner.GetGuid(),
            ownerClass.GetName(),
            normalizedPath,
            beforeBlob,
            std::move(afterBlob),
            true);
        if (!commitResult.Ok)
        {
            outcome.bSceneResult = true;
            outcome.SceneResult = commitResult;
            return outcome;
        }

        outcome.Ok = true;
        return outcome;
    }

    AutomationResponse EditorAutomationSurface::HandleSetProperty(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string objectId = request.Payload.value("objectId", "");
        const bool batchForm = request.Payload.contains("properties");
        if (objectId.empty())
        {
            return MakeError(request, "InvalidPayload", "objectId is required.");
        }

        if (!batchForm
            && (request.Payload.value("propertyPath", "").empty()
                || !request.Payload.contains("value")))
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

        const Serialization::SerializerOptions options = sceneEditor->GetPropertyCommandSerializerOptions();

        if (!batchForm)
        {
            const std::string normalizedPath =
                ScenePropertyValueCodec::NormalizePropertyPath(request.Payload.value("propertyPath", ""));
            const Json& rawValue = request.Payload.at("value");

            Json value;
            ScenePropertyValueCodec::RefError refError = ScenePropertyValueCodec::RefError::None;
            std::string refErrorMessage;
            if (!ScenePropertyValueCodec::TryNormalizeValueReferences(
                    rawValue, value, refError, refErrorMessage))
            {
                return MakeError(
                    request,
                    MapRefErrorCode(refError),
                    refErrorMessage,
                    Json{{"objectId", objectId}, {"propertyPath", normalizedPath}});
            }

            std::vector<uint8_t> beforeBlob;
            std::string codecError;
            if (!ScenePropertyValueCodec::TryCaptureBlob(
                    *owner, *ownerClass, normalizedPath, beforeBlob, codecError, options))
            {
                return MakeError(request, "InvalidPropertyPath", codecError);
            }

            const PropertyWriteOutcome outcome = WritePropertyValue(
                context,
                *session,
                *sceneEditor,
                *owner,
                *ownerClass,
                normalizedPath,
                value,
                beforeBlob);
            if (!outcome.Ok)
            {
                if (outcome.bSceneResult)
                {
                    return FromEditorResult(request, outcome.SceneResult);
                }
                return MakeError(
                    request,
                    outcome.ErrorCode,
                    outcome.ErrorMessage,
                    Json{{"objectId", objectId}, {"propertyPath", normalizedPath}});
            }

            Json result = Json{
                {"objectId", objectId},
                {"propertyPath", normalizedPath},
                {"value", value}};
            if (outcome.bUnchanged)
            {
                result["unchanged"] = true;
            }
            return FromEditorResult(request, MakeOk(), std::move(result));
        }

        // Batch form. Phase A resolves every reference before anything is written, so an
        // unresolvable reference rejects the whole request with the live objects untouched.
        const Json& items = request.Payload["properties"];
        if (!items.is_array() || items.empty())
        {
            return MakeError(request, "InvalidPayload", "properties must be a non-empty array.");
        }

        struct BatchEntry
        {
            std::string PropertyPath;
            Json Value;
            std::vector<uint8_t> BeforeBlob;
        };

        std::vector<BatchEntry> entries;
        entries.reserve(items.size());
        std::unordered_map<std::string, size_t> seenPaths;

        for (size_t index = 0; index < items.size(); ++index)
        {
            const Json& item = items[index];
            const Json location = Json{{"index", index}};
            if (!item.is_object() || !item.contains("propertyPath") || !item["propertyPath"].is_string()
                || !item.contains("value"))
            {
                Json extra = location;
                extra["objectId"] = objectId;
                return MakeError(request, "InvalidPayload", "Each properties[] item needs propertyPath and value.", extra);
            }

            BatchEntry entry;
            entry.PropertyPath =
                ScenePropertyValueCodec::NormalizePropertyPath(item["propertyPath"].get<std::string>());
            if (entry.PropertyPath.empty())
            {
                Json extra = location;
                extra["objectId"] = objectId;
                return MakeError(request, "InvalidPayload", "properties[].propertyPath is empty.", extra);
            }

            if (!seenPaths.emplace(entry.PropertyPath, index).second)
            {
                Json extra = location;
                extra["objectId"] = objectId;
                extra["propertyPath"] = entry.PropertyPath;
                return MakeError(
                    request,
                    "InvalidPayload",
                    "Duplicate propertyPath in one request: " + entry.PropertyPath,
                    extra);
            }

            ScenePropertyValueCodec::RefError refError = ScenePropertyValueCodec::RefError::None;
            std::string refErrorMessage;
            if (!ScenePropertyValueCodec::TryNormalizeValueReferences(
                    item["value"], entry.Value, refError, refErrorMessage))
            {
                Json extra = location;
                extra["objectId"] = objectId;
                extra["propertyPath"] = entry.PropertyPath;
                return MakeError(request, MapRefErrorCode(refError), refErrorMessage, extra);
            }

            std::string codecError;
            if (!ScenePropertyValueCodec::TryCaptureBlob(
                    *owner, *ownerClass, entry.PropertyPath, entry.BeforeBlob, codecError, options))
            {
                Json extra = location;
                extra["objectId"] = objectId;
                extra["propertyPath"] = entry.PropertyPath;
                return MakeError(request, "InvalidPropertyPath", codecError, extra);
            }

            entries.push_back(std::move(entry));
        }

        // Phase B writes in order. A failing write is reported as PartialWrite with the entries
        // that already landed: the engine has no generic setter rollback, so no claim is made.
        Json applied = Json::array();
        for (size_t index = 0; index < entries.size(); ++index)
        {
            const BatchEntry& entry = entries[index];
            const PropertyWriteOutcome outcome = WritePropertyValue(
                context,
                *session,
                *sceneEditor,
                *owner,
                *ownerClass,
                entry.PropertyPath,
                entry.Value,
                entry.BeforeBlob);
            if (!outcome.Ok)
            {
                Json failed = Json{{"index", index}, {"propertyPath", entry.PropertyPath}};
                if (outcome.bSceneResult)
                {
                    failed["code"] = MapErrorCode(outcome.SceneResult.Code);
                    failed["message"] = outcome.SceneResult.Message;
                }
                else
                {
                    failed["code"] = outcome.ErrorCode;
                    failed["message"] = outcome.ErrorMessage;
                }

                return MakeError(
                    request,
                    "PartialWrite",
                    "Property write failed after " + std::to_string(applied.size())
                        + " applied propert(ies); earlier writes are not rolled back.",
                    Json{{"objectId", objectId}, {"applied", applied}, {"failed", failed}});
            }

            Json appliedEntry = Json{{"propertyPath", entry.PropertyPath}, {"value", entry.Value}};
            if (outcome.bUnchanged)
            {
                appliedEntry["unchanged"] = true;
            }
            applied.push_back(std::move(appliedEntry));
        }

        return FromEditorResult(
            request,
            MakeOk(),
            Json{{"objectId", objectId}, {"applied", applied}});
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
        const std::string runtimeClassName = request.Payload.value(
            "runtimeClassName",
            request.Payload.value("className", ""));
        const AssetEditorResult createResult = context.GetAssetEditorServices().Create(
            request.Payload.value("assetType", ""),
            request.Payload.value("directory", ""),
            request.Payload.value("name", ""),
            path,
            guid,
            runtimeClassName);
        if (!createResult.Ok)
        {
            return MakeError(request, "InvalidPayload", createResult.Message);
        }

        Json result = Json{{"path", path}, {"guid", guid}};
        if (!runtimeClassName.empty())
        {
            result["runtimeClassName"] = runtimeClassName;
        }
        else if (request.Payload.value("assetType", "") == "DataAsset")
        {
            if (const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(path))
            {
                result["runtimeClassName"] = meta->RuntimeClassName;
            }
        }
        return FromEditorResult(request, MakeOk(), std::move(result));
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
        if (atlasPath.empty())
        {
            return MakeError(request, "InvalidPayload", "atlasPath is required.");
        }

        std::string loadError;
        std::shared_ptr<SpriteAtlas> atlas = std::dynamic_pointer_cast<SpriteAtlas>(
            AssetManager::Get().LoadAssetByPath(atlasPath, loadError));
        if (atlas == nullptr)
        {
            return MakeError(
                request,
                "InvalidPayload",
                "Failed to load SpriteAtlas: " + atlasPath
                    + (loadError.empty() ? "" : (" (" + loadError + ")")));
        }

        std::vector<std::shared_ptr<Sprite>> sprites;
        if (request.Payload.contains("spritePaths") && request.Payload["spritePaths"].is_array())
        {
            for (const Json& item : request.Payload["spritePaths"])
            {
                if (!item.is_string())
                {
                    return MakeError(
                        request,
                        "InvalidPayload",
                        "spritePaths entries must be strings.");
                }

                const std::string spritePath = item.get<std::string>();
                std::string spriteLoadError;
                std::shared_ptr<Sprite> sprite = std::dynamic_pointer_cast<Sprite>(
                    AssetManager::Get().LoadAssetByPath(spritePath, spriteLoadError));
                if (sprite == nullptr)
                {
                    return MakeError(
                        request,
                        "InvalidPayload",
                        "Failed to load Sprite '" + spritePath
                            + (spriteLoadError.empty() ? "'." : ("': " + spriteLoadError)));
                }
                sprites.push_back(std::move(sprite));
            }
        }

        const size_t memberCountBefore = atlas->GetMembers().size();
        // Local stack: this op persists immediately, so there is no cross-RPC undo session.
        EditorCommandStack commandStack;
        const GraphEditorResult addResult =
            context.GetSpriteEditorServices().AddAtlasMembers(commandStack, *atlas, sprites);
        if (!addResult.Ok)
        {
            return MakeError(request, "InternalError", addResult.Message);
        }

        const GraphEditorResult saveResult = context.GetSpriteEditorServices().SaveAtlas(*atlas);
        if (!saveResult.Ok)
        {
            return MakeError(request, "InternalError", saveResult.Message);
        }

        const size_t added = atlas->GetMembers().size() - memberCountBefore;
        return FromEditorResult(
            request,
            MakeOk(),
            Json{
                {"added", added},
                {"memberCount", atlas->GetMembers().size()},
                {"saved", true}});
    }

    AutomationResponse EditorAutomationSurface::HandleSpriteAtlasSetPadding(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string atlasPath =
            request.Payload.value("atlasPath", request.Payload.value("path", ""));
        if (atlasPath.empty())
        {
            return MakeError(request, "InvalidPayload", "atlasPath is required.");
        }

        std::string loadError;
        std::shared_ptr<SpriteAtlas> atlas = std::dynamic_pointer_cast<SpriteAtlas>(
            AssetManager::Get().LoadAssetByPath(atlasPath, loadError));
        if (atlas == nullptr)
        {
            return MakeError(
                request,
                "InvalidPayload",
                "Failed to load SpriteAtlas: " + atlasPath
                    + (loadError.empty() ? "" : (" (" + loadError + ")")));
        }

        const int32_t padding = request.Payload.value("padding", atlas->GetPadding());
        SpriteAtlasPaddingMode mode = atlas->GetPaddingMode();
        const std::string modeStr = request.Payload.value("paddingMode", "");
        if (modeStr == "DilateBorder" || modeStr == "dilate")
        {
            mode = SpriteAtlasPaddingMode::DilateBorder;
        }
        else if (modeStr == "PadWithZero" || modeStr == "zero")
        {
            mode = SpriteAtlasPaddingMode::PadWithZero;
        }
        else if (!modeStr.empty())
        {
            return MakeError(
                request,
                "InvalidPayload",
                "Unknown paddingMode '" + modeStr
                    + "' (expected DilateBorder/dilate or PadWithZero/zero)");
        }

        const GraphEditorResult setResult =
            context.GetSpriteEditorServices().SetAtlasPadding(*atlas, padding, mode);
        if (!setResult.Ok)
        {
            return MakeError(request, "InternalError", setResult.Message);
        }

        const GraphEditorResult saveResult = context.GetSpriteEditorServices().SaveAtlas(*atlas);
        if (!saveResult.Ok)
        {
            return MakeError(request, "InternalError", saveResult.Message);
        }

        return FromEditorResult(
            request,
            MakeOk(),
            Json{
                {"padding", padding},
                {"paddingMode",
                 mode == SpriteAtlasPaddingMode::DilateBorder ? "DilateBorder" : "PadWithZero"},
                {"saved", true}});
    }

    AutomationResponse EditorAutomationSurface::HandleSpriteAtlasSetSampler(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string atlasPath =
            request.Payload.value("atlasPath", request.Payload.value("path", ""));
        if (atlasPath.empty())
        {
            return MakeError(request, "InvalidPayload", "atlasPath is required.");
        }

        std::string loadError;
        std::shared_ptr<SpriteAtlas> atlas = std::dynamic_pointer_cast<SpriteAtlas>(
            AssetManager::Get().LoadAssetByPath(atlasPath, loadError));
        if (atlas == nullptr)
        {
            return MakeError(
                request,
                "InvalidPayload",
                "Failed to load SpriteAtlas: " + atlasPath
                    + (loadError.empty() ? "" : (" (" + loadError + ")")));
        }

        RHISamplerFilter filter = atlas->GetSamplerFilter();
        RHISamplerAddress address = atlas->GetSamplerAddress();

        const std::string filterStr = request.Payload.value(
            "filter",
            request.Payload.value("samplerFilter", ""));
        if (!filterStr.empty())
        {
            if (filterStr == "Nearest")
            {
                filter = RHISamplerFilter::Nearest;
            }
            else if (filterStr == "Linear")
            {
                filter = RHISamplerFilter::Linear;
            }
            else
            {
                return MakeError(
                    request,
                    "InvalidPayload",
                    "Unknown filter '" + filterStr + "' (expected Nearest or Linear)");
            }
        }

        const std::string addressStr = request.Payload.value(
            "address",
            request.Payload.value("samplerAddress", ""));
        if (!addressStr.empty())
        {
            if (addressStr == "Repeat")
            {
                address = RHISamplerAddress::Repeat;
            }
            else if (addressStr == "MirroredRepeat")
            {
                address = RHISamplerAddress::MirroredRepeat;
            }
            else if (addressStr == "ClampToEdge")
            {
                address = RHISamplerAddress::ClampToEdge;
            }
            else if (addressStr == "ClampToBorder")
            {
                address = RHISamplerAddress::ClampToBorder;
            }
            else
            {
                return MakeError(
                    request,
                    "InvalidPayload",
                    "Unknown address '" + addressStr
                        + "' (expected Repeat, MirroredRepeat, ClampToEdge, or ClampToBorder)");
            }
        }

        const GraphEditorResult setResult =
            context.GetSpriteEditorServices().SetAtlasSampler(*atlas, filter, address);
        if (!setResult.Ok)
        {
            return MakeError(request, "InternalError", setResult.Message);
        }

        const GraphEditorResult saveResult = context.GetSpriteEditorServices().SaveAtlas(*atlas);
        if (!saveResult.Ok)
        {
            return MakeError(request, "InternalError", saveResult.Message);
        }

        const char* addressName = "ClampToEdge";
        switch (address)
        {
        case RHISamplerAddress::Repeat:
            addressName = "Repeat";
            break;
        case RHISamplerAddress::MirroredRepeat:
            addressName = "MirroredRepeat";
            break;
        case RHISamplerAddress::ClampToEdge:
            addressName = "ClampToEdge";
            break;
        case RHISamplerAddress::ClampToBorder:
            addressName = "ClampToBorder";
            break;
        }
        return FromEditorResult(
            request,
            MakeOk(),
            Json{
                {"filter", filter == RHISamplerFilter::Nearest ? "Nearest" : "Linear"},
                {"address", addressName},
                {"saved", true}});
    }

    AutomationResponse EditorAutomationSurface::HandleSpriteAtlasRebuild(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string atlasPath =
            request.Payload.value("atlasPath", request.Payload.value("path", ""));
        if (atlasPath.empty())
        {
            return MakeError(request, "InvalidPayload", "atlasPath is required.");
        }

        std::string loadError;
        std::shared_ptr<SpriteAtlas> atlas = std::dynamic_pointer_cast<SpriteAtlas>(
            AssetManager::Get().LoadAssetByPath(atlasPath, loadError));
        if (atlas == nullptr)
        {
            return MakeError(
                request,
                "InvalidPayload",
                "Failed to load SpriteAtlas: " + atlasPath
                    + (loadError.empty() ? "" : (" (" + loadError + ")")));
        }

        if (!RenderSystem::HasInstance() || RenderSystem::Get().GetRHI() == nullptr)
        {
            return MakeError(request, "InternalError", "RHI is not available.");
        }

        const GraphEditorResult rebuildResult =
            context.GetSpriteEditorServices().RebuildAtlas(*atlas, *RenderSystem::Get().GetRHI());
        if (!rebuildResult.Ok)
        {
            return MakeError(request, "InternalError", rebuildResult.Message);
        }

        const GraphEditorResult saveResult = context.GetSpriteEditorServices().SaveAtlas(*atlas);
        if (!saveResult.Ok)
        {
            return MakeError(request, "InternalError", saveResult.Message);
        }

        for (const std::shared_ptr<Sprite>& sprite : atlas->GetMembers())
        {
            if (!sprite)
            {
                continue;
            }
            const GraphEditorResult spriteSave =
                context.GetSpriteEditorServices().SaveSprite(*sprite);
            if (!spriteSave.Ok)
            {
                return MakeError(
                    request,
                    "InternalError",
                    "Rebuild succeeded but cascade save for sprite '" + sprite->GetName()
                        + "' failed: " + spriteSave.Message);
            }
        }

        return FromEditorResult(request, MakeOk(), Json{{"rebuilt", true}, {"saved", true}});
    }

    AutomationResponse EditorAutomationSurface::HandleSpriteAtlasSave(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string atlasPath =
            request.Payload.value("atlasPath", request.Payload.value("path", ""));
        if (atlasPath.empty())
        {
            return MakeError(request, "InvalidPayload", "atlasPath is required.");
        }

        std::string loadError;
        std::shared_ptr<SpriteAtlas> atlas = std::dynamic_pointer_cast<SpriteAtlas>(
            AssetManager::Get().LoadAssetByPath(atlasPath, loadError));
        if (atlas == nullptr)
        {
            return MakeError(
                request,
                "InvalidPayload",
                "Failed to load SpriteAtlas: " + atlasPath
                    + (loadError.empty() ? "" : (" (" + loadError + ")")));
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

    AutomationResponse EditorAutomationSurface::HandleCreatePrefabFromGameObject(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string objectId = request.Payload.value("objectId", "");
        const std::string directory = request.Payload.value("directory", "");
        const std::string name = request.Payload.value("name", "");
        const bool overwrite = request.Payload.value("overwrite", false);
        if (objectId.empty() || directory.empty() || name.empty())
        {
            return MakeError(request, "InvalidPayload", "objectId, directory, and name are required.");
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

        SceneEditor* sceneEditor = dynamic_cast<SceneEditor*>(context.FindSubModule(SceneEditor::kModuleId));
        if (sceneEditor == nullptr)
        {
            return MakeError(request, "InternalError", "SceneEditor module is unavailable.");
        }

        std::string constraintError;
        if (!PrefabEditConstraints::AllowCreatePrefab(*sceneEditor, &constraintError))
        {
            return MakeError(request, "PrefabConstrained", constraintError);
        }

        if (context.IsPlaying())
        {
            return MakeError(request, "PlayMode", "Prefab authoring is unavailable during Play.");
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

        const std::filesystem::path contentRoot = PathRegistry::Get().GetProjectContentRoot();
        if (contentRoot.empty())
        {
            return MakeError(request, "InternalError", "ProjectContentRoot is not set.");
        }

        const std::filesystem::path directoryAbsolute =
            (contentRoot / std::filesystem::path(directory)).lexically_normal();
        std::error_code directoryError;
        if (!std::filesystem::is_directory(directoryAbsolute, directoryError))
        {
            return MakeError(
                request,
                "DirectoryNotFound",
                "Directory does not exist: " + directory,
                Json{{"directory", directory}});
        }

        const std::string prefabPath =
            (std::filesystem::path(directory) / (name + kPrefabAssetExtension)).generic_string();
        const AssetMeta* existingMeta = AssetManager::Get().FindAssetMetaByPath(prefabPath);
        const std::filesystem::path prefabAbsolutePath =
            AssetManager::Get().ResolveAssetAbsolutePath(prefabPath);
        std::error_code fileError;
        const bool assetFileExists = std::filesystem::is_regular_file(prefabAbsolutePath, fileError);
        const bool targetExists = existingMeta != nullptr || assetFileExists;
        if (targetExists && !overwrite)
        {
            return MakeError(
                request,
                "AssetExists",
                "Asset already exists (pass overwrite=true to replace it): " + prefabPath,
                Json{{"prefabPath", prefabPath}});
        }

        if (gameObject->GetRootComponent() == nullptr)
        {
            return MakeError(
                request,
                "InvalidPayload",
                "'" + gameObject->GetName() + "' needs a root SceneComponent before it can become a Prefab.",
                Json{{"objectId", objectId}});
        }

        // The engine's create path registers the source object as a Prefab instance of the new
        // asset. Automation must leave the scene untouched, so that record is dropped again.
        std::vector<PrefabInstanceRecord>& instanceRecords = scene->GetPrefabInstancesMutable();
        const size_t instanceRecordCountBefore = instanceRecords.size();

        PrefabCreateReport report;
        std::shared_ptr<Prefab> created = PrefabUtility::CreatePrefabFromGameObject(*gameObject, &report);
        if (instanceRecords.size() > instanceRecordCountBefore
            && instanceRecords.back().RootInstanceGuid == gameObject->GetGuid())
        {
            instanceRecords.pop_back();
        }

        if (!created)
        {
            return MakeError(request, "SaveFailed", "CreatePrefabFromGameObject failed.", Json{{"prefabPath", prefabPath}});
        }

        std::shared_ptr<Prefab> prefabToSave = created;
        if (targetExists)
        {
            std::string loadError;
            std::shared_ptr<Asset> existingAsset =
                AssetManager::Get().LoadAssetByPath(prefabPath, loadError);
            std::shared_ptr<Prefab> existingPrefab = Cast<Prefab>(existingAsset);
            if (existingPrefab == nullptr)
            {
                std::string message = "Existing Prefab asset could not be loaded: " + prefabPath;
                if (!loadError.empty())
                {
                    message += " (" + loadError + ")";
                }
                return MakeError(
                    request,
                    "SaveFailed",
                    message,
                    Json{{"prefabPath", prefabPath}});
            }

            // Reuse the existing asset object so its Guid (and every reference to it) survives
            // the rebuild; only the template tree is replaced.
            const std::vector<std::shared_ptr<GameObject>> newTemplates = created->GetTemplateObjects();
            const GUID newRootGuid = created->GetRootGuid();
            created->ClearTemplateObjects();
            existingPrefab->ClearTemplateObjects();
            for (const std::shared_ptr<GameObject>& templateObject : newTemplates)
            {
                if (templateObject == nullptr)
                {
                    continue;
                }

                templateObject->SetOuter(existingPrefab.get());
                existingPrefab->AddTemplateObject(templateObject);
            }
            existingPrefab->SetRootGuid(newRootGuid);
            prefabToSave = existingPrefab;
        }

        AssetFileRollback rollback;
        rollback.Capture(prefabAbsolutePath);

        std::string saveError;
        if (!PrefabUtility::SavePrefabAsset(*prefabToSave, prefabPath, &saveError))
        {
            rollback.Restore();
            return MakeError(
                request,
                "SaveFailed",
                saveError.empty() ? "SavePrefabAsset failed." : saveError,
                Json{{"prefabPath", prefabPath}});
        }

        GameObject* templateRoot = prefabToSave->GetRootGameObject();
        return FromEditorResult(
            request,
            MakeOk(),
            Json{
                {"prefabPath", prefabPath},
                {"guid", prefabToSave->GetGuid().ToString()},
                {"rootName", templateRoot != nullptr ? templateRoot->GetName() : name},
                {"overwritten", targetExists}});
    }

    AutomationResponse EditorAutomationSurface::HandlePrefabOpen(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string prefabPath = request.Payload.value("prefabPath", "");
        if (prefabPath.empty())
        {
            return MakeError(request, "InvalidPayload", "prefabPath is required.");
        }

        if (!AssetManager::HasInstance())
        {
            return MakeError(request, "InternalError", "AssetManager is not available.");
        }

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(prefabPath);
        if (meta == nullptr)
        {
            return MakeError(
                request,
                "AssetNotFound",
                "No imported asset at '" + prefabPath + "'.",
                Json{{"prefabPath", prefabPath}});
        }
        if (meta->AssetType != "Prefab")
        {
            return MakeError(
                request,
                "InvalidPayload",
                "'" + meta->AssetPath + "' is a " + meta->AssetType + " asset, not a Prefab.",
                Json{{"prefabPath", meta->AssetPath}});
        }

        SceneEditor* sceneEditor = dynamic_cast<SceneEditor*>(context.FindSubModule(SceneEditor::kModuleId));
        if (sceneEditor == nullptr)
        {
            return MakeError(request, "InternalError", "SceneEditor module is unavailable.");
        }

        PrefabStageController& stages = sceneEditor->GetPrefabStages();
        const std::string previousKey = stages.GetActiveAssetKey();
        const bool alreadyOpen = stages.HasStage(meta->AssetPath);

        // Only one Stage stays active. A clean previous Stage is closed; a dirty one is kept so
        // unsaved edits are never discarded silently, and the response says so.
        bool previousStageClosed = false;
        bool previousStageDirty = false;
        if (!previousKey.empty() && previousKey != meta->AssetPath)
        {
            previousStageDirty = stages.IsDirty(previousKey);
            sceneEditor->ExitPrefabStage();
            if (!previousStageDirty)
            {
                stages.Discard(previousKey);
                previousStageClosed = true;
            }
        }

        std::string openError;
        if (!stages.OpenFromAsset(*meta, &openError))
        {
            return MakeError(
                request,
                "PrefabOpenFailed",
                openError,
                Json{{"prefabPath", meta->AssetPath}});
        }

        EditorDocumentHost& host = context.GetDocumentHost();
        if (!host.OpenOrFocus(*meta))
        {
            return MakeError(
                request,
                "PrefabOpenFailed",
                "Failed to open the Prefab document: " + meta->AssetPath,
                Json{{"prefabPath", meta->AssetPath}});
        }

        EditorDocumentSession* session = host.FindSessionByAssetKey(meta->AssetPath);
        if (session == nullptr)
        {
            return MakeError(
                request,
                "InternalError",
                "Prefab document session is missing after open.",
                Json{{"prefabPath", meta->AssetPath}});
        }

        PrefabEditorStage* stage = stages.FindStage(meta->AssetPath);
        if (stage == nullptr || stage->StageScene == nullptr)
        {
            return MakeError(
                request,
                "PrefabOpenFailed",
                "Prefab Stage was not created: " + meta->AssetPath,
                Json{{"prefabPath", meta->AssetPath}});
        }

        const std::string rootGuid = FindStageRootGuid(*stage->StageScene);
        if (rootGuid.empty())
        {
            return MakeError(
                request,
                "PrefabOpenFailed",
                "Prefab Stage has no root GameObject: " + meta->AssetPath,
                Json{{"prefabPath", meta->AssetPath}});
        }

        return FromEditorResult(
            request,
            MakeOk(),
            Json{
                {"prefabPath", meta->AssetPath},
                {"sessionId", AutomationProtocol::FormatSessionId(session->GetId().Value)},
                {"rootGuid", rootGuid},
                {"guid", meta->Guid.ToString()},
                {"opened", true},
                {"alreadyOpen", alreadyOpen},
                {"previousStageClosed", previousStageClosed},
                {"previousStageDirty", previousStageDirty}});
    }

    AutomationResponse EditorAutomationSurface::HandlePrefabSave(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string prefabPath = request.Payload.value("prefabPath", "");
        if (prefabPath.empty())
        {
            return MakeError(request, "InvalidPayload", "prefabPath is required.");
        }

        if (!AssetManager::HasInstance())
        {
            return MakeError(request, "InternalError", "AssetManager is not available.");
        }

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(prefabPath);
        if (meta == nullptr)
        {
            return MakeError(
                request,
                "AssetNotFound",
                "No imported asset at '" + prefabPath + "'.",
                Json{{"prefabPath", prefabPath}});
        }

        SceneEditor* sceneEditor = dynamic_cast<SceneEditor*>(context.FindSubModule(SceneEditor::kModuleId));
        if (sceneEditor == nullptr)
        {
            return MakeError(request, "InternalError", "SceneEditor module is unavailable.");
        }

        PrefabStageController& stages = sceneEditor->GetPrefabStages();
        if (!stages.HasStage(meta->AssetPath))
        {
            return MakeError(
                request,
                "PrefabNotOpen",
                "No Prefab Stage is open for '" + meta->AssetPath + "'; call prefab.open first.",
                Json{{"prefabPath", meta->AssetPath}});
        }

        const std::filesystem::path prefabAbsolutePath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta->AssetPath);
        AssetFileRollback rollback;
        rollback.Capture(prefabAbsolutePath);

        std::string saveError;
        if (!stages.SaveStage(meta->AssetPath, context, &saveError))
        {
            rollback.Restore();
            return MakeError(
                request,
                "SaveFailed",
                saveError.empty() ? "Prefab Stage save failed." : saveError,
                Json{{"prefabPath", meta->AssetPath}});
        }

        return FromEditorResult(
            request,
            MakeOk(),
            Json{
                {"prefabPath", meta->AssetPath},
                {"saved", true},
                {"guid", meta->Guid.ToString()}});
    }

    AutomationResponse EditorAutomationSurface::HandlePrefabClose(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string prefabPath = request.Payload.value("prefabPath", "");
        const bool bDiscard = request.Payload.value("discard", false);
        if (prefabPath.empty())
        {
            return MakeError(request, "InvalidPayload", "prefabPath is required.");
        }

        if (!AssetManager::HasInstance())
        {
            return MakeError(request, "InternalError", "AssetManager is not available.");
        }

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(prefabPath);
        if (meta == nullptr)
        {
            return MakeError(
                request,
                "AssetNotFound",
                "No imported asset at '" + prefabPath + "'.",
                Json{{"prefabPath", prefabPath}});
        }
        if (meta->AssetType != "Prefab")
        {
            return MakeError(
                request,
                "InvalidPayload",
                "'" + meta->AssetPath + "' is a " + meta->AssetType + " asset, not a Prefab.",
                Json{{"prefabPath", meta->AssetPath}});
        }

        SceneEditor* sceneEditor = dynamic_cast<SceneEditor*>(context.FindSubModule(SceneEditor::kModuleId));
        if (sceneEditor == nullptr)
        {
            return MakeError(request, "InternalError", "SceneEditor module is unavailable.");
        }

        PrefabStageController& stages = sceneEditor->GetPrefabStages();
        if (!stages.HasStage(meta->AssetPath))
        {
            return MakeError(
                request,
                "PrefabNotOpen",
                "No Prefab Stage is open for '" + meta->AssetPath + "'.",
                Json{{"prefabPath", meta->AssetPath}});
        }

        const bool bDirty = stages.IsDirty(meta->AssetPath);
        if (bDirty && !bDiscard)
        {
            return MakeError(
                request,
                "UnsavedChanges",
                "Prefab Stage has unsaved changes; save it or pass discard=true.",
                Json{{"prefabPath", meta->AssetPath}, {"dirty", true}});
        }

        // A discarded Stage must not look dirty, otherwise the document close path would raise a
        // modal unsaved-changes dialog. Clean Stages close exactly like a human tab close.
        if (bDiscard)
        {
            stages.ClearDirty(meta->AssetPath);
        }

        EditorDocumentHost& host = context.GetDocumentHost();
        if (EditorDocumentSession* session = host.FindSessionByAssetKey(meta->AssetPath))
        {
            host.RequestClose(session->GetId());
        }

        // No document session (Stage opened outside a Prefab tab): tear the Stage down directly.
        if (stages.HasStage(meta->AssetPath))
        {
            if (stages.GetActiveAssetKey() == meta->AssetPath)
            {
                sceneEditor->ExitPrefabStage();
            }
            stages.Discard(meta->AssetPath);
        }

        // A discarded Stage scene is destroyed: never leave the editor inspecting freed memory.
        if (!stages.HasActiveStage())
        {
            Scene* documentScene = sceneEditor->GetDocumentScene();
            Scene* inspectingScene = context.GetInspectingScene();
            if (documentScene != nullptr && inspectingScene != nullptr && inspectingScene != documentScene)
            {
                context.SetInspectingScene(documentScene);
                sceneEditor->SyncSelectionWithScene();
            }
        }

        return FromEditorResult(
            request,
            MakeOk(),
            Json{
                {"prefabPath", meta->AssetPath},
                {"closed", true},
                {"discarded", bDirty && bDiscard}});
    }

}
