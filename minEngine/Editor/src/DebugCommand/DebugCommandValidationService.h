#pragma once

#include "DebugCommand/DebugCommandRegistry.h"
#include "DebugCommand/DebugCommandSetValueValidation.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine::DebugCommand
{
    struct DebugCommandValidationError
    {
        std::string Message;
        std::vector<std::string> Suggestions;
    };

    class DebugCommandValidationService
    {
    public:
        static std::optional<DebugCommandValidationError> ValidateCommandArgs(
            const DebugCommandRegistry::StoredCommand& command,
            const std::vector<std::string>& args);

        static std::optional<DebugCommandValidationError> ValidateSetValue(
            const DebugCommandContext& context,
            std::string_view propertyPathText,
            std::string_view valueLiteral);

        static std::string FormatMachineFriendlyMessage(
            std::string_view summary,
            const std::vector<std::string>& suggestions);

        static DebugCommandResult BuildCommandError(const DebugCommandValidationError& error);
    };
}
