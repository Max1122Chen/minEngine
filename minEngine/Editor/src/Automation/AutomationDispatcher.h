#pragma once

#include "Automation/CapabilityRegistry.h"
#include "Automation/IEditorAutomationSurface.h"

#include <string>

namespace minEngine
{
    class IEditorContext;

    class AutomationDispatcher
    {
    public:
        AutomationDispatcher(CapabilityRegistry& capabilities, IEditorAutomationSurface& surface);

        AutomationResponse Dispatch(IEditorContext& context, const std::string& requestJson) const;

    private:
        CapabilityRegistry& m_Capabilities;
        IEditorAutomationSurface& m_Surface;

        static bool TryParseRequest(const std::string& requestJson, AutomationRequest& outRequest, std::string& outError);
        AutomationResponse MakeParseError(const std::string& requestId, const std::string& code, const std::string& message) const;
    };
}
