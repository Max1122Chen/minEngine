#include "Automation/IEditorAutomationSurface.h"

namespace minEngine
{
    Json AutomationResponse::ToJson() const
    {
        Json json = {
            {"protocolVersion", ProtocolVersion},
            {"requestId", RequestId},
            {"ok", Ok},
            {"errors", Errors}};
        if (!Result.is_null())
        {
            json["result"] = Result;
        }
        if (!Warnings.empty())
        {
            json["warnings"] = Warnings;
        }
        return json;
    }
}
