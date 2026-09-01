#include "Runtime/Core/Command/CommandExecutor.h"

#include "Runtime/Core/Command/CommandRegistry.h"

namespace minEngine::Command
{
    std::vector<std::string> CommandExecutor::TokenizeLine(std::string_view line)
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

    CommandResult CommandExecutor::Execute(std::string_view commandId,
                                           const std::vector<std::string>& args,
                                           const CommandContext& context) const
    {
        const CommandRegistry::StoredCommand* command = CommandRegistry::Get().Find(commandId);
        if (command == nullptr)
        {
            CommandOutputBuilder builder;
            builder.AddLine(CommandOutputKind::Error, "Error: unknown command '" + std::string(commandId) + "'");
            return builder.BuildError("unknown command");
        }

        CommandResult result = command->Execute(context, args);
        if (result.Message.empty() && !result.Lines.empty())
        {
            CommandOutputBuilder flattenBuilder;
            for (const CommandOutputLine& line : result.Lines)
            {
                for (const CommandOutputSegment& segment : line.Segments)
                {
                    flattenBuilder.AddSegment(segment.Kind, segment.Text);
                }
                flattenBuilder.NewLine();
            }
            result.Message = flattenBuilder.FlattenToPlainText();
        }
        return result;
    }

    CommandResult CommandExecutor::ExecuteLine(std::string_view line, const CommandContext& context) const
    {
        const std::vector<std::string> tokens = TokenizeLine(line);
        if (tokens.empty())
        {
            return CommandResult::MakeOk();
        }

        std::vector<std::string> args;
        if (tokens.size() > 1)
        {
            args.assign(tokens.begin() + 1, tokens.end());
        }

        return Execute(tokens.front(), args, context);
    }
}
