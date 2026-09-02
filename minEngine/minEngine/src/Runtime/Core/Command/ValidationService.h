#pragma once

#include "Runtime/Core/Command/CommandRegistry.h"
#include "Runtime/Core/Command/SetValueValidation.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine::Command
{
    struct ValidationError
    {
        std::string Message;
        std::vector<std::string> Suggestions;
    };

    class ValidationService
    {
    public:
        static std::optional<ValidationError> ValidateCommandArgs(
            const CommandRegistry::StoredCommand& command,
            const std::vector<std::string>& args);

        static std::optional<ValidationError> ValidateSetValue(
            const CommandContext& context,
            std::string_view propertyPathText,
            std::string_view valueLiteral);

        static std::string FormatMachineFriendlyMessage(
            std::string_view summary,
            const std::vector<std::string>& suggestions);

        static CommandResult BuildCommandError(const ValidationError& error);
    };
}
