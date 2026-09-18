#pragma once

#include "Automation/CapabilityRegistry.h"
#include "Automation/IEditorAutomationSurface.h"
#include "Services/Scene/SceneEditorResult.h"
#include "Services/Scene/SceneEditorServices.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace minEngine
{
    class EditorDocumentSession;
    class GameObject;

    class EditorAutomationSurface : public IEditorAutomationSurface
    {
    public:
        EditorAutomationSurface(CapabilityRegistry& capabilities, SceneEditorServices& sceneServices);

        AutomationResponse Handle(IEditorContext& context, const AutomationRequest& request) override;

    private:
        using OpHandler = std::function<AutomationResponse(IEditorContext&, const AutomationRequest&)>;

        CapabilityRegistry& m_Capabilities;
        SceneEditorServices& m_SceneServices;
        std::unordered_map<std::string, OpHandler> m_Handlers;

        void RegisterHandlers();
        void AssertHandlersCoverCapabilities() const;

        AutomationResponse MakeError(
            const AutomationRequest& request,
            const std::string& code,
            const std::string& message,
            Json extraFields = Json::object()) const;

        AutomationResponse FromEditorResult(
            const AutomationRequest& request,
            const SceneEditorResult& result,
            Json okResult = Json::object()) const;

        static const char* MapErrorCode(SceneEditorError code);

        EditorDocumentSession* ResolveSession(
            IEditorContext& context,
            const AutomationRequest& request,
            AutomationResponse& outErrorResponse) const;

        Json BuildHierarchyNode(GameObject& gameObject, int depthRemaining) const;

        AutomationResponse HandlePing(IEditorContext& context, const AutomationRequest& request) const;
        AutomationResponse HandleGetState(IEditorContext& context, const AutomationRequest& request) const;
        AutomationResponse HandleGetCapabilities(IEditorContext& context, const AutomationRequest& request) const;
        AutomationResponse HandleGetHierarchy(IEditorContext& context, const AutomationRequest& request);
        AutomationResponse HandleGetObject(IEditorContext& context, const AutomationRequest& request);
        AutomationResponse HandleGetProperty(IEditorContext& context, const AutomationRequest& request);
        AutomationResponse HandleCreateGameObject(IEditorContext& context, const AutomationRequest& request);
        AutomationResponse HandleDeleteGameObject(IEditorContext& context, const AutomationRequest& request);
        AutomationResponse HandleAddComponent(IEditorContext& context, const AutomationRequest& request);
        AutomationResponse HandleRemoveComponent(IEditorContext& context, const AutomationRequest& request);
        AutomationResponse HandleSetProperty(IEditorContext& context, const AutomationRequest& request);
        AutomationResponse HandleReparent(IEditorContext& context, const AutomationRequest& request);
        AutomationResponse HandleSave(IEditorContext& context, const AutomationRequest& request);
        AutomationResponse HandleValidate(IEditorContext& context, const AutomationRequest& request);
    };
}
