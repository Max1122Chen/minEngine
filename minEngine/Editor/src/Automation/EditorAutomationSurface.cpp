#include "Automation/EditorAutomationSurface.h"

#include "Automation/AutomationProtocol.h"
#include "Core.h"
#include "Runtime/Core/EngineVersion.h"
#include "Runtime/Core/ProductBranding.h"
#include "Runtime/Function/Framework/Project/ProjectManager.h"
#include "Shell/Document/EditorDocumentHost.h"
#include "Shell/IEditorContext.h"

#include <filesystem>

namespace minEngine
{
    EditorAutomationSurface::EditorAutomationSurface(
        CapabilityRegistry& capabilities,
        SceneAutomationServices& sceneServices)
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

    AutomationResponse EditorAutomationSurface::FromSceneResult(
        const AutomationRequest& request,
        SceneServiceResult&& serviceResult) const
    {
        if (!serviceResult.Ok)
        {
            return MakeError(
                request,
                serviceResult.ErrorCode.empty() ? "InternalError" : serviceResult.ErrorCode,
                serviceResult.Message,
                std::move(serviceResult.ExtraErrorFields));
        }

        AutomationResponse response;
        response.ProtocolVersion = kAutomationProtocolVersion;
        response.RequestId = request.RequestId;
        response.Ok = true;
        response.Result = std::move(serviceResult.Result);
        return response;
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
        int maxDepth = -1;
        if (request.Payload.contains("maxDepth") && request.Payload["maxDepth"].is_number_integer())
        {
            maxDepth = request.Payload["maxDepth"].get<int>();
        }
        return FromSceneResult(request, m_SceneServices.GetHierarchy(context, request.SessionId, maxDepth));
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
        return FromSceneResult(request, m_SceneServices.GetObject(context, request.SessionId, objectId));
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

        return FromSceneResult(
            request,
            m_SceneServices.GetProperty(context, request.SessionId, objectId, propertyPath));
    }

    AutomationResponse EditorAutomationSurface::HandleCreateGameObject(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string name = request.Payload.value("name", "");
        const std::string parentObjectId = request.Payload.value("parentObjectId", "");
        return FromSceneResult(
            request,
            m_SceneServices.CreateGameObject(context, request.SessionId, name, parentObjectId));
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
        return FromSceneResult(
            request,
            m_SceneServices.DeleteGameObject(context, request.SessionId, objectId));
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
        return FromSceneResult(
            request,
            m_SceneServices.AddComponent(context, request.SessionId, objectId, className));
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
        return FromSceneResult(
            request,
            m_SceneServices.RemoveComponent(context, request.SessionId, componentObjectId));
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

        return FromSceneResult(
            request,
            m_SceneServices.SetProperty(
                context,
                request.SessionId,
                objectId,
                propertyPath,
                request.Payload.at("value")));
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
        const std::string newParentObjectId = request.Payload.value("newParentObjectId", "");
        return FromSceneResult(
            request,
            m_SceneServices.Reparent(context, request.SessionId, objectId, newParentObjectId));
    }

    AutomationResponse EditorAutomationSurface::HandleSave(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        return FromSceneResult(request, m_SceneServices.SaveDocument(context, request.SessionId));
    }

    AutomationResponse EditorAutomationSurface::HandleValidate(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string objectId = request.Payload.value("objectId", "");
        const std::string propertyPath = request.Payload.value("propertyPath", "");
        return FromSceneResult(
            request,
            m_SceneServices.Validate(context, request.SessionId, objectId, propertyPath));
    }
}
