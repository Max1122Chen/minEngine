#include "DebugCommand/DebugCommandValidationService.h"

#include "PropertyPath/PropertyPath.h"
#include "Runtime/Core/Reflection/MEEnum.h"

#include <sstream>

namespace minEngine::DebugCommand
{
    namespace
    {
        size_t CountRequiredArgs(const std::vector<DebugCommandArgDescriptor>& args)
        {
            size_t requiredCount = 0;
            for (const DebugCommandArgDescriptor& arg : args)
            {
                if (arg.Required)
                {
                    ++requiredCount;
                }
            }

            return requiredCount;
        }

        size_t EffectiveSetArgCount(const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                return 0;
            }

            if (args.size() >= 3 && args[1] == "=")
            {
                return args.size() - 1;
            }

            return args.size();
        }

        std::string_view ExtractSetValueLiteral(const std::vector<std::string>& args)
        {
            if (args.size() < 2)
            {
                return {};
            }

            size_t valueTokenIndex = 1;
            if (args.size() > 2 && args[1] == "=")
            {
                valueTokenIndex = 2;
            }

            if (args.size() <= valueTokenIndex)
            {
                return {};
            }

            return args[valueTokenIndex];
        }

        std::string ExpectedTypeLabel(const PropertySetValueInfo& info)
        {
            switch (info.Kind)
            {
                case PropertySetValueKind::Bool:
                    return "bool";
                case PropertySetValueKind::SignedInteger:
                    return "integer";
                case PropertySetValueKind::UnsignedInteger:
                    return "unsigned integer";
                case PropertySetValueKind::Float:
                    return "float";
                case PropertySetValueKind::Double:
                    return "double";
                case PropertySetValueKind::String:
                    return "string";
                case PropertySetValueKind::Enum:
                    return info.EnumType != nullptr ? info.EnumType->GetName() : "enum";
                default:
                    return "value";
            }
        }

        std::vector<std::string> BuildValueSuggestions(const PropertySetValueInfo& info)
        {
            std::vector<std::string> suggestions;
            switch (info.Kind)
            {
                case PropertySetValueKind::Bool:
                    suggestions.push_back("true");
                    suggestions.push_back("false");
                    break;
                case PropertySetValueKind::Enum:
                    if (info.EnumType != nullptr)
                    {
                        for (const Reflection::MEEnumEntry& entry : info.EnumType->GetEntries())
                        {
                            suggestions.push_back(entry.name);
                        }
                    }
                    break;
                default:
                    break;
            }

            return suggestions;
        }

        std::string BuildExpectedGotMessage(std::string_view expected, std::string_view got)
        {
            std::string message = "expected ";
            message.append(expected);
            message.append(", got '");
            message.append(got);
            message.push_back('\'');
            return message;
        }
    }

    std::string DebugCommandValidationService::FormatMachineFriendlyMessage(
        std::string_view summary,
        const std::vector<std::string>& suggestions)
    {
        if (suggestions.empty())
        {
            return std::string(summary);
        }

        std::ostringstream stream;
        stream << summary << " suggestions: [";
        for (size_t index = 0; index < suggestions.size(); ++index)
        {
            if (index > 0)
            {
                stream << ", ";
            }

            stream << suggestions[index];
        }

        stream << ']';
        return stream.str();
    }

    DebugCommandResult DebugCommandValidationService::BuildCommandError(const DebugCommandValidationError& error)
    {
        DebugCommandOutputBuilder builder;
        builder.AddLine(
            DebugCommandOutputKind::Error,
            "Error: " + FormatMachineFriendlyMessage(error.Message, error.Suggestions));
        return builder.BuildError(error.Message);
    }

    std::optional<DebugCommandValidationError> DebugCommandValidationService::ValidateCommandArgs(
        const DebugCommandRegistry::StoredCommand& command,
        const std::vector<std::string>& args)
    {
        if (command.Args.empty())
        {
            return std::nullopt;
        }

        const size_t requiredCount = CountRequiredArgs(command.Args);
        const size_t providedCount =
            command.Id == "set" ? EffectiveSetArgCount(args) : args.size();
        if (providedCount < requiredCount)
        {
            DebugCommandValidationError error;
            error.Message = "command '" + command.Id + "' requires " + std::to_string(requiredCount)
                + " argument(s), got " + std::to_string(providedCount);
            return error;
        }

        if (command.Id == "set")
        {
            const std::string_view valueLiteral = ExtractSetValueLiteral(args);
            if (valueLiteral.empty())
            {
                DebugCommandValidationError error;
                error.Message = "set requires a value literal";
                return error;
            }
        }

        return std::nullopt;
    }

    std::optional<DebugCommandValidationError> DebugCommandValidationService::ValidateSetValue(
        const DebugCommandContext& context,
        std::string_view propertyPathText,
        std::string_view valueLiteral)
    {
        const PropertyValueValidation validation =
            DebugCommandSetValueValidation::ValidateSetLiteral(context, propertyPathText, valueLiteral);
        if (validation.State != PropertyValueValidationState::Invalid)
        {
            return std::nullopt;
        }

        DebugCommandValidationError error;
        error.Message = validation.Message.empty()
            ? BuildExpectedGotMessage(ExpectedTypeLabel(validation.ValueInfo), valueLiteral)
            : validation.Message;
        error.Suggestions = validation.Suggestions;
        if (error.Suggestions.empty())
        {
            error.Suggestions = BuildValueSuggestions(validation.ValueInfo);
        }

        if (validation.Message.empty())
        {
            error.Message = BuildExpectedGotMessage(ExpectedTypeLabel(validation.ValueInfo), valueLiteral);
        }

        return error;
    }
}
