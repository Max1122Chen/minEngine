#include "Automation/EditorAutomationSurface.h"

#include "Automation/AutomationProtocol.h"
#include "Commands/EditorObjectPropertyApply.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Animation/AnimationGraph.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialEdGraphNode.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "Services/Animation/AnimationGraphEditorServices.h"
#include "Services/Material/MaterialEditorServices.h"
#include "Services/Scene/ScenePropertyValueCodec.h"
#include "Shell/Document/EditorDocumentHost.h"
#include "Shell/Document/EditorDocumentSession.h"
#include "Shell/IEditorContext.h"

namespace minEngine
{
    const char* EditorAutomationSurface::MapGraphError(GraphEditorError code)
    {
        switch (code)
        {
        case GraphEditorError::None:
            return "Ok";
        case GraphEditorError::NotFound:
            return "InvalidObjectId";
        case GraphEditorError::InvalidArgument:
            return "InvalidPayload";
        case GraphEditorError::InvalidPropertyPath:
            return "InvalidPropertyPath";
        case GraphEditorError::Internal:
        default:
            return "InternalError";
        }
    }

    AutomationResponse EditorAutomationSurface::FromGraphResult(
        const AutomationRequest& request,
        const GraphEditorResult& result,
        Json okResult) const
    {
        if (!result.Ok)
        {
            return MakeError(request, MapGraphError(result.Code), result.Message);
        }

        AutomationResponse response;
        response.ProtocolVersion = kAutomationProtocolVersion;
        response.RequestId = request.RequestId;
        response.Ok = true;
        response.Result = std::move(okResult);
        if (!result.Message.empty())
        {
            response.Result["message"] = result.Message;
        }
        return response;
    }

    EditorDocumentSession* EditorAutomationSurface::ResolveSessionOfType(
        IEditorContext& context,
        const AutomationRequest& request,
        std::string_view typeId,
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

        if (session->GetTypeId() != typeId)
        {
            outErrorResponse = MakeError(
                request,
                "SessionNotFound",
                std::string("Session is not a ") + std::string(typeId) + " document.");
            return nullptr;
        }

        return session;
    }

    Json EditorAutomationSurface::BuildMaterialGraphJson(const Material& material) const
    {
        Json nodes = Json::array();
        if (material.m_Graph)
        {
            for (const std::shared_ptr<MaterialEdGraphNode>& node : material.m_Graph->m_Nodes)
            {
                if (!node || node->GetNodeDef() == nullptr)
                {
                    continue;
                }

                MaterialGraphNodeDef* nodeDef = node->GetNodeDef();
                Json inputs = Json::array();
                for (int32_t inputIndex = 0; inputIndex < nodeDef->GetInputCount(); ++inputIndex)
                {
                    const MaterialGraphNodeDefInput* input = nodeDef->GetInput(inputIndex);
                    if (input == nullptr)
                    {
                        continue;
                    }

                    Json inputJson = {
                        {"index", inputIndex},
                        {"name", input->Name},
                        {"outputIndex", input->OutputIndex}};
                    if (input->ConnectedNodeDefGuid != GUID::Zero())
                    {
                        inputJson["connectedNodeId"] = input->ConnectedNodeDefGuid.ToString();
                    }
                    inputs.push_back(std::move(inputJson));
                }

                Json outputs = Json::array();
                for (int32_t outputIndex = 0; outputIndex < nodeDef->GetOutputCount(); ++outputIndex)
                {
                    const MaterialGraphNodeDefOutput* output = nodeDef->GetOutput(outputIndex);
                    if (output == nullptr)
                    {
                        continue;
                    }
                    outputs.push_back(Json{{"index", outputIndex}, {"name", output->Name}});
                }

                const Reflection::MEClass* nodeClass = nodeDef->GetClass();
                nodes.push_back(Json{
                    {"nodeId", nodeDef->GetGuid().ToString()},
                    {"className", nodeClass != nullptr ? nodeClass->GetName() : std::string()},
                    {"editorPosX", node->m_EditorPosX},
                    {"editorPosY", node->m_EditorPosY},
                    {"inputs", std::move(inputs)},
                    {"outputs", std::move(outputs)}});
            }
        }

        return Json{
            {"shadingModel", static_cast<int>(material.m_ShadingModel)},
            {"blendMode", static_cast<int>(material.m_BlendMode)},
            {"nodes", std::move(nodes)}};
    }

    Json EditorAutomationSurface::BuildAnimGraphJson(const AnimationGraph& graph) const
    {
        const AnimStateMachine& stateMachine = graph.GetStateMachine();
        Json states = Json::array();
        for (const AnimState& state : stateMachine.States)
        {
            states.push_back(Json{
                {"name", state.Name},
                {"editorPosX", state.EditorPosX},
                {"editorPosY", state.EditorPosY},
                {"loop", state.bLoop},
                {"speed", state.Speed}});
        }

        Json transitions = Json::array();
        for (size_t index = 0; index < stateMachine.Transitions.size(); ++index)
        {
            const AnimTransition& transition = stateMachine.Transitions[index];
            transitions.push_back(Json{
                {"index", index},
                {"from", transition.FromStateName},
                {"to", transition.ToStateName},
                {"blendDurationSeconds", transition.BlendDurationSeconds}});
        }

        Json anyStateTransitions = Json::array();
        for (size_t index = 0; index < stateMachine.AnyStateTransitions.size(); ++index)
        {
            const AnimTransition& transition = stateMachine.AnyStateTransitions[index];
            anyStateTransitions.push_back(Json{
                {"index", index},
                {"to", transition.ToStateName},
                {"blendDurationSeconds", transition.BlendDurationSeconds}});
        }

        return Json{
            {"defaultState", stateMachine.DefaultStateName},
            {"states", std::move(states)},
            {"transitions", std::move(transitions)},
            {"anyStateTransitions", std::move(anyStateTransitions)}};
    }

    AutomationResponse EditorAutomationSurface::HandleGetMaterialGraph(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSessionOfType(context, request, "Material", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        Material* material = nullptr;
        const GraphEditorResult result =
            context.GetMaterialEditorServices().GetMaterial(context, *session, &material);
        if (!result.Ok || material == nullptr)
        {
            return FromGraphResult(request, result);
        }

        Json graph = BuildMaterialGraphJson(*material);
        graph["assetPath"] = session->GetAssetKey();
        return FromGraphResult(request, result, std::move(graph));
    }

    AutomationResponse EditorAutomationSurface::HandleMaterialAddNode(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string className = request.Payload.value("className", "");
        if (className.empty())
        {
            return MakeError(request, "InvalidPayload", "className is required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSessionOfType(context, request, "Material", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        const float editorPosX = request.Payload.value("editorPosX", 0.0f);
        const float editorPosY = request.Payload.value("editorPosY", 0.0f);
        GUID created;
        const GraphEditorResult result = context.GetMaterialEditorServices().AddNode(
            context,
            *session,
            className,
            editorPosX,
            editorPosY,
            &created);
        if (!result.Ok)
        {
            return FromGraphResult(request, result);
        }

        return FromGraphResult(request, result, Json{{"nodeId", created.ToString()}});
    }

    AutomationResponse EditorAutomationSurface::HandleMaterialRemoveNode(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string nodeId = request.Payload.value("nodeId", "");
        GUID guid;
        if (nodeId.empty() || !ScenePropertyValueCodec::TryParseGuid(nodeId, guid))
        {
            return MakeError(request, "InvalidPayload", "nodeId is required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSessionOfType(context, request, "Material", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        const GraphEditorResult result =
            context.GetMaterialEditorServices().RemoveNode(context, *session, guid);
        return FromGraphResult(request, result, Json{{"nodeId", nodeId}});
    }

    AutomationResponse EditorAutomationSurface::HandleMaterialConnectPins(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string fromNodeId = request.Payload.value("fromNodeId", "");
        const std::string toNodeId = request.Payload.value("toNodeId", "");
        if (!request.Payload.contains("fromOutputIndex") || !request.Payload.contains("toInputIndex"))
        {
            return MakeError(request, "InvalidPayload", "fromOutputIndex and toInputIndex are required.");
        }

        GUID fromGuid;
        GUID toGuid;
        if (!ScenePropertyValueCodec::TryParseGuid(fromNodeId, fromGuid)
            || !ScenePropertyValueCodec::TryParseGuid(toNodeId, toGuid))
        {
            return MakeError(request, "InvalidPayload", "fromNodeId and toNodeId must be GUIDs.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSessionOfType(context, request, "Material", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        const GraphEditorResult result = context.GetMaterialEditorServices().ConnectPins(
            context,
            *session,
            fromGuid,
            request.Payload.at("fromOutputIndex").get<int32_t>(),
            toGuid,
            request.Payload.at("toInputIndex").get<int32_t>());
        return FromGraphResult(request, result);
    }

    AutomationResponse EditorAutomationSurface::HandleMaterialDisconnectInput(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string toNodeId = request.Payload.value("toNodeId", "");
        if (!request.Payload.contains("toInputIndex"))
        {
            return MakeError(request, "InvalidPayload", "toNodeId and toInputIndex are required.");
        }

        GUID toGuid;
        if (!ScenePropertyValueCodec::TryParseGuid(toNodeId, toGuid))
        {
            return MakeError(request, "InvalidPayload", "toNodeId must be a GUID.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSessionOfType(context, request, "Material", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        const GraphEditorResult result = context.GetMaterialEditorServices().DisconnectInput(
            context,
            *session,
            toGuid,
            request.Payload.at("toInputIndex").get<int32_t>());
        return FromGraphResult(request, result);
    }

    AutomationResponse EditorAutomationSurface::HandleMaterialSave(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSessionOfType(context, request, "Material", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        const GraphEditorResult result = context.GetMaterialEditorServices().SaveDocument(context, *session);
        return FromGraphResult(request, result, Json{{"saved", result.Ok}});
    }

    AutomationResponse EditorAutomationSurface::HandleMaterialCompile(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSessionOfType(context, request, "Material", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        const GraphEditorResult result = context.GetMaterialEditorServices().Compile(context, *session);
        return FromGraphResult(request, result, Json{{"compiled", result.Ok}});
    }

    AutomationResponse EditorAutomationSurface::HandleGetAnimGraph(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        AutomationResponse errorResponse;
        EditorDocumentSession* session =
            ResolveSessionOfType(context, request, "AnimationGraph", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        AnimationGraph* graph = nullptr;
        const GraphEditorResult result =
            context.GetAnimationGraphEditorServices().GetAnimationGraph(context, *session, &graph);
        if (!result.Ok || graph == nullptr)
        {
            return FromGraphResult(request, result);
        }

        Json document = BuildAnimGraphJson(*graph);
        document["assetPath"] = session->GetAssetKey();
        return FromGraphResult(request, result, std::move(document));
    }

    AutomationResponse EditorAutomationSurface::HandleAnimAddState(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        AutomationResponse errorResponse;
        EditorDocumentSession* session =
            ResolveSessionOfType(context, request, "AnimationGraph", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        std::string stateName;
        const GraphEditorResult result = context.GetAnimationGraphEditorServices().AddState(
            context,
            *session,
            request.Payload.value("editorPosX", 0.0f),
            request.Payload.value("editorPosY", 0.0f),
            &stateName);
        return FromGraphResult(request, result, Json{{"stateName", stateName}});
    }

    AutomationResponse EditorAutomationSurface::HandleAnimRemoveState(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string stateName = request.Payload.value("stateName", "");
        if (stateName.empty())
        {
            return MakeError(request, "InvalidPayload", "stateName is required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session =
            ResolveSessionOfType(context, request, "AnimationGraph", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        const GraphEditorResult result =
            context.GetAnimationGraphEditorServices().RemoveState(context, *session, stateName);
        return FromGraphResult(request, result, Json{{"stateName", stateName}});
    }

    AutomationResponse EditorAutomationSurface::HandleAnimRenameState(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string oldName = request.Payload.value("oldName", "");
        const std::string newName = request.Payload.value("newName", "");
        if (oldName.empty() || newName.empty())
        {
            return MakeError(request, "InvalidPayload", "oldName and newName are required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session =
            ResolveSessionOfType(context, request, "AnimationGraph", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        const GraphEditorResult result = context.GetAnimationGraphEditorServices().RenameState(
            context,
            *session,
            oldName,
            newName);
        return FromGraphResult(request, result, Json{{"stateName", newName}});
    }

    AutomationResponse EditorAutomationSurface::HandleAnimAddTransition(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string fromState = request.Payload.value("fromState", "");
        const std::string toState = request.Payload.value("toState", "");
        if (fromState.empty() || toState.empty())
        {
            return MakeError(request, "InvalidPayload", "fromState and toState are required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session =
            ResolveSessionOfType(context, request, "AnimationGraph", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        size_t index = 0;
        const GraphEditorResult result = context.GetAnimationGraphEditorServices().AddTransition(
            context,
            *session,
            fromState,
            toState,
            &index);
        return FromGraphResult(request, result, Json{{"transitionIndex", index}});
    }

    AutomationResponse EditorAutomationSurface::HandleAnimAddAnyStateTransition(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string toState = request.Payload.value("toState", "");
        if (toState.empty())
        {
            return MakeError(request, "InvalidPayload", "toState is required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session =
            ResolveSessionOfType(context, request, "AnimationGraph", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        size_t index = 0;
        const GraphEditorResult result = context.GetAnimationGraphEditorServices().AddAnyStateTransition(
            context,
            *session,
            toState,
            &index);
        return FromGraphResult(request, result, Json{{"transitionIndex", index}});
    }

    AutomationResponse EditorAutomationSurface::HandleAnimRemoveTransition(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        if (!request.Payload.contains("transitionIndex"))
        {
            return MakeError(request, "InvalidPayload", "transitionIndex is required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session =
            ResolveSessionOfType(context, request, "AnimationGraph", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        const int index = request.Payload.at("transitionIndex").get<int>();
        if (index < 0)
        {
            return MakeError(request, "InvalidPayload", "transitionIndex is out of range.");
        }

        const GraphEditorResult result = context.GetAnimationGraphEditorServices().RemoveTransition(
            context,
            *session,
            static_cast<size_t>(index));
        return FromGraphResult(request, result);
    }

    AutomationResponse EditorAutomationSurface::HandleAnimRemoveAnyStateTransition(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        if (!request.Payload.contains("transitionIndex"))
        {
            return MakeError(request, "InvalidPayload", "transitionIndex is required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session =
            ResolveSessionOfType(context, request, "AnimationGraph", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        const int index = request.Payload.at("transitionIndex").get<int>();
        if (index < 0)
        {
            return MakeError(request, "InvalidPayload", "transitionIndex is out of range.");
        }

        const GraphEditorResult result = context.GetAnimationGraphEditorServices().RemoveAnyStateTransition(
            context,
            *session,
            static_cast<size_t>(index));
        return FromGraphResult(request, result);
    }

    AutomationResponse EditorAutomationSurface::HandleAnimSetDefaultState(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        const std::string stateName = request.Payload.value("stateName", "");
        if (stateName.empty())
        {
            return MakeError(request, "InvalidPayload", "stateName is required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session =
            ResolveSessionOfType(context, request, "AnimationGraph", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        const GraphEditorResult result =
            context.GetAnimationGraphEditorServices().SetDefaultState(context, *session, stateName);
        return FromGraphResult(request, result, Json{{"stateName", stateName}});
    }

    AutomationResponse EditorAutomationSurface::HandleMaterialSetProperty(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        return HandleGraphSetProperty(context, request, "Material", true);
    }

    AutomationResponse EditorAutomationSurface::HandleAnimSetProperty(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        return HandleGraphSetProperty(context, request, "AnimationGraph", false);
    }

    AutomationResponse EditorAutomationSurface::HandleAnimSave(
        IEditorContext& context,
        const AutomationRequest& request)
    {
        AutomationResponse errorResponse;
        EditorDocumentSession* session =
            ResolveSessionOfType(context, request, "AnimationGraph", errorResponse);
        if (session == nullptr)
        {
            return errorResponse;
        }

        const GraphEditorResult result =
            context.GetAnimationGraphEditorServices().SaveDocument(context, *session);
        return FromGraphResult(request, result, Json{{"saved", result.Ok}});
    }

    AutomationResponse EditorAutomationSurface::HandleGraphSetProperty(
        IEditorContext& context,
        const AutomationRequest& request,
        std::string_view sessionTypeId,
        bool materialDomain)
    {
        const std::string objectId = request.Payload.value("objectId", "");
        const std::string propertyPath = request.Payload.value("propertyPath", "");
        if (objectId.empty() || propertyPath.empty() || !request.Payload.contains("value"))
        {
            return MakeError(request, "InvalidPayload", "objectId, propertyPath, and value are required.");
        }

        AutomationResponse errorResponse;
        EditorDocumentSession* session = ResolveSessionOfType(context, request, sessionTypeId, errorResponse);
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
        if (!owner || owner->GetClass() == nullptr)
        {
            return MakeError(request, "InvalidObjectId", "No object with this GUID.");
        }

        const std::string normalizedPath = ScenePropertyValueCodec::NormalizePropertyPath(propertyPath);
        const Serialization::SerializerOptions options = EditorObjectPropertyApply::MakeDefaultOptions();
        std::string codecError;
        std::vector<uint8_t> beforeBlob;
        if (!ScenePropertyValueCodec::TryCaptureBlob(
                *owner,
                *owner->GetClass(),
                normalizedPath,
                beforeBlob,
                codecError,
                options))
        {
            return MakeError(request, "InvalidPropertyPath", codecError);
        }

        if (!ScenePropertyValueCodec::TryWriteJsonToLive(
                *owner,
                *owner->GetClass(),
                normalizedPath,
                request.Payload.at("value"),
                codecError,
                options))
        {
            ScenePropertyValueCodec::TryRestoreBlob(
                *owner,
                *owner->GetClass(),
                normalizedPath,
                beforeBlob,
                codecError,
                options);
            return MakeError(request, "InvalidPayload", codecError);
        }

        std::vector<uint8_t> afterBlob;
        if (!ScenePropertyValueCodec::TryCaptureBlob(
                *owner,
                *owner->GetClass(),
                normalizedPath,
                afterBlob,
                codecError,
                options))
        {
            ScenePropertyValueCodec::TryRestoreBlob(
                *owner,
                *owner->GetClass(),
                normalizedPath,
                beforeBlob,
                codecError,
                options);
            return MakeError(request, "InternalError", codecError);
        }

        if (!ScenePropertyValueCodec::TryRestoreBlob(
                *owner,
                *owner->GetClass(),
                normalizedPath,
                beforeBlob,
                codecError,
                options))
        {
            return MakeError(request, "InternalError", "Failed to restore pre-edit value: " + codecError);
        }

        if (beforeBlob == afterBlob)
        {
            GraphEditorResult unchanged;
            unchanged.Ok = true;
            return FromGraphResult(
                request,
                unchanged,
                Json{{"objectId", objectId}, {"propertyPath", normalizedPath}, {"unchanged", true}});
        }

        const std::string ownerClassName = owner->GetClass()->GetName();
        const GraphEditorResult result = materialDomain
            ? context.GetMaterialEditorServices().CommitPropertyBlobs(
                  context,
                  *session,
                  guid,
                  ownerClassName,
                  normalizedPath,
                  std::move(beforeBlob),
                  std::move(afterBlob),
                  true)
            : context.GetAnimationGraphEditorServices().CommitPropertyBlobs(
                  context,
                  *session,
                  guid,
                  ownerClassName,
                  normalizedPath,
                  std::move(beforeBlob),
                  std::move(afterBlob),
                  true);
        return FromGraphResult(
            request,
            result,
            Json{{"objectId", objectId}, {"propertyPath", normalizedPath}});
    }
}
