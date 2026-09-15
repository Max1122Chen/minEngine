#include "DebugCommand/DebugCommandExecutor.h"

#include "DebugCommand/DebugCommandRegistry.h"
#include "DebugCommand/DebugCommandValidationService.h"

namespace minEngine::DebugCommand
{
    std::vector<std::string> DebugCommandExecutor::TokenizeLine(std::string_view line)
    {
        std::vector<std::string> tokens;
        std::string currentToken;
        bool inQuotes = false;

        for (size_t index = 0; index < line.size(); ++index)
        {
            const char character = line[index];
            if (character == '"')
            {
                inQuotes = !inQuotes;
                continue;
            }

            if (!inQuotes && (character == ' ' || character == '\t'))
            {
                if (!currentToken.empty())
                {
                    tokens.push_back(currentToken);
                    currentToken.clear();
                }
                continue;
            }

            currentToken.push_back(character);
        }

        if (!currentToken.empty())
        {
            tokens.push_back(currentToken);
        }

        return tokens;
    }

    DebugCommandResult DebugCommandExecutor::Execute(std::string_view commandId,
                                           const std::vector<std::string>& args,
                                           const DebugCommandContext& context) const
    {
        const DebugCommandRegistry::StoredCommand* command = DebugCommandRegistry::Get().Find(commandId);
        if (command == nullptr)
        {
            DebugCommandOutputBuilder builder;
            builder.AddLine(DebugCommandOutputKind::Error, "Error: unknown command '" + std::string(commandId) + "'");
            return builder.BuildError("unknown command");
        }

        if (const std::optional<DebugCommandValidationError> argError = DebugCommandValidationService::ValidateCommandArgs(*command, args))
        {
            return DebugCommandValidationService::BuildCommandError(*argError);
        }

        DebugCommandResult result = command->Execute(context, args);
        if (result.Message.empty() && !result.Lines.empty())
        {
            DebugCommandOutputBuilder flattenBuilder;
            for (const DebugCommandOutputLine& line : result.Lines)
            {
                for (const DebugCommandOutputSegment& segment : line.Segments)
                {
                    flattenBuilder.AddSegment(segment.Kind, segment.Text);
                }
                flattenBuilder.NewLine();
            }
            result.Message = flattenBuilder.FlattenToPlainText();
        }
        return result;
    }

    DebugCommandResult DebugCommandExecutor::ExecuteLine(std::string_view line, const DebugCommandContext& context) const
    {
        const std::vector<std::string> tokens = TokenizeLine(line);
        if (tokens.empty())
        {
            return DebugCommandResult::MakeOk();
        }

        std::vector<std::string> args;
        if (tokens.size() > 1)
        {
            args.assign(tokens.begin() + 1, tokens.end());
        }

        return Execute(tokens.front(), args, context);
    }
}
