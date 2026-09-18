#pragma once

#include "Runtime/Core/Serialization/Json.h"

#include <string>

namespace minEngine
{
    class IEditorContext;

    struct AutomationRequest
    {
        int ProtocolVersion = 0;
        std::string RequestId;
        std::string Kind;
        std::string Op;
        std::string SessionId;
        Json Payload = Json::object();
    };

    struct AutomationResponse
    {
        int ProtocolVersion = 1;
        std::string RequestId;
        bool Ok = false;
        Json Result = nullptr;
        Json Errors = Json::array();
        Json Warnings = Json::array();

        Json ToJson() const;
    };

    class IEditorAutomationSurface
    {
    public:
        virtual ~IEditorAutomationSurface() = default;

        virtual AutomationResponse Handle(IEditorContext& context, const AutomationRequest& request) = 0;
    };
}
