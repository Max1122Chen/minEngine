#include "Automation/AutomationDispatcher.h"

#include "Automation/AutomationProtocol.h"

namespace minEngine
{
    AutomationDispatcher::AutomationDispatcher(CapabilityRegistry& capabilities, IEditorAutomationSurface& surface)
        : m_Capabilities(capabilities)
        , m_Surface(surface)
    {
    }

    AutomationResponse AutomationDispatcher::MakeParseError(
        const std::string& requestId,
        const std::string& code,
        const std::string& message) const
    {
        AutomationResponse response;
        response.ProtocolVersion = kAutomationProtocolVersion;
        response.RequestId = requestId;
        response.Ok = false;
        response.Errors.push_back(Json{{"code", code}, {"message", message}});
        return response;
    }

    bool AutomationDispatcher::TryParseRequest(
        const std::string& requestJson,
        AutomationRequest& outRequest,
        std::string& outError)
    {
        Json root;
        try
        {
            root = Json::parse(requestJson);
        }
        catch (const Json::parse_error& parseError)
        {
            outError = parseError.what();
            return false;
        }

        if (!root.is_object())
        {
            outError = "Request root must be a JSON object.";
            return false;
        }

        outRequest.ProtocolVersion = root.value("protocolVersion", 0);
        outRequest.RequestId = root.value("requestId", "");
        outRequest.Kind = root.value("kind", "");
        outRequest.Op = root.value("op", "");
        outRequest.SessionId = root.value("sessionId", "");
        if (root.contains("payload") && root["payload"].is_object())
        {
            outRequest.Payload = root["payload"];
        }
        else
        {
            outRequest.Payload = Json::object();
        }
        return true;
    }

    AutomationResponse AutomationDispatcher::Dispatch(IEditorContext& context, const std::string& requestJson) const
    {
        AutomationRequest request;
        std::string parseError;
        if (!TryParseRequest(requestJson, request, parseError))
        {
            return MakeParseError("", "InvalidPayload", parseError);
        }

        if (request.RequestId.empty())
        {
            return MakeParseError("", "InvalidPayload", "requestId is required.");
        }

        if (request.ProtocolVersion != kAutomationProtocolVersion)
        {
            return MakeParseError(
                request.RequestId,
                "ProtocolVersionMismatch",
                "Unsupported protocolVersion; expected "
                    + std::to_string(kAutomationProtocolVersion) + ".");
        }

        if (request.Op.empty() || request.Kind.empty())
        {
            return MakeParseError(request.RequestId, "InvalidPayload", "kind and op are required.");
        }

        const AutomationCapability* capability = m_Capabilities.FindByOp(request.Op);
        if (capability == nullptr)
        {
            AutomationResponse response = MakeParseError(
                request.RequestId,
                "UnknownOp",
                "Unknown op: " + request.Op);
            response.Errors.front()["op"] = request.Op;
            return response;
        }

        if (capability->Kind != request.Kind)
        {
            AutomationResponse response = MakeParseError(
                request.RequestId,
                "KindOpMismatch",
                "Request kind '" + request.Kind + "' does not match registered kind '"
                    + capability->Kind + "' for op '" + request.Op + "'.");
            response.Errors.front()["op"] = request.Op;
            return response;
        }

        if (capability->SessionRequired && request.SessionId.empty())
        {
            return MakeParseError(
                request.RequestId,
                "SessionNotFound",
                "sessionId is required for op '" + request.Op + "'.");
        }

        return m_Surface.Handle(context, request);
    }
}
