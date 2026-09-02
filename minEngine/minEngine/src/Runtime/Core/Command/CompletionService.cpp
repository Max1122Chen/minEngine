#include "Runtime/Core/Command/CompletionService.h"

#include "Runtime/Core/Command/CommandRegistry.h"
#include "Runtime/Core/Command/SceneCommandUtils.h"
#include "Runtime/Core/Command/SetValueValidation.h"

namespace minEngine::Command
{
    namespace
    {
        std::vector<std::string> TokenizeLine(std::string_view line)
        {
            std::vector<std::string> tokens;
            std::string currentToken;
            for (const char character : line)
            {
                if (character == ' ' || character == '\t')
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

        bool EndsWithPartialToken(std::string_view line)
        {
            if (line.empty())
            {
                return false;
            }

            return line.back() != ' ' && line.back() != '\t';
        }

        std::string_view CurrentToken(std::string_view line)
        {
            const size_t lastSpace = line.find_last_of(" \t");
            if (lastSpace == std::string_view::npos)
            {
                return line;
            }

            return line.substr(lastSpace + 1);
        }

        bool IsSetValuePhase(const std::vector<std::string>& tokens, bool hasPartialToken)
        {
            if (tokens.size() >= 3)
            {
                return true;
            }

            return tokens.size() == 2 && !hasPartialToken;
        }

        bool IsGetInspectPathComplete(const std::vector<std::string>& tokens, bool hasPartialToken)
        {
            if (tokens.size() >= 3)
            {
                return true;
            }

            return tokens.size() == 2 && !hasPartialToken;
        }

        void AppendCommandCompletions(std::string_view prefix, std::vector<CompletionItem>& outItems)
        {
            const std::vector<const CommandRegistry::StoredCommand*> commands =
                CommandRegistry::Get().List(prefix, CommandScope::Both);
            for (const CommandRegistry::StoredCommand* command : commands)
            {
                if (command == nullptr)
                {
                    continue;
                }

                CompletionItem item;
                item.Label = command->Id;
                item.InsertText = command->Id;
                item.Description = command->Description;
                item.Kind = CompletionKind::Command;
                outItems.push_back(std::move(item));
            }
        }

        void AppendGameObjectCompletions(
            const CommandContext& context,
            std::string_view prefix,
            std::vector<CompletionItem>& outItems)
        {
            for (const std::string& objectName : SceneCommandUtils::ListGameObjectNames(context.ActiveScene, prefix))
            {
                CompletionItem item;
                item.Label = objectName;
                item.InsertText = objectName;
                item.Description = "GameObject";
                item.Kind = CompletionKind::ObjectRef;
                outItems.push_back(std::move(item));
            }
        }
    }

    std::vector<CompletionItem> CompletionService::Complete(
        std::string_view line,
        size_t cursorOffset,
        const CommandContext& context)
    {
        (void)cursorOffset;

        std::vector<CompletionItem> items;
        const std::string_view trimmedLine = line;
        if (trimmedLine.empty())
        {
            AppendCommandCompletions({}, items);
            return items;
        }

        const std::vector<std::string> tokens = TokenizeLine(trimmedLine);
        const std::string_view currentToken = CurrentToken(trimmedLine);
        const bool hasPartialToken = EndsWithPartialToken(trimmedLine);

        if (tokens.empty() || (tokens.size() == 1 && hasPartialToken))
        {
            AppendCommandCompletions(currentToken, items);
            return items;
        }

        const std::string& commandId = tokens.front();
        if (commandId == "get" || commandId == "set" || commandId == "inspect")
        {
            if (commandId == "set" && IsSetValuePhase(tokens, hasPartialToken))
            {
                const SetValuePhase valuePhase = SetValueValidation::ParseValuePhase(trimmedLine);
                return SetValueValidation::CompleteValue(context, valuePhase);
            }

            if (commandId != "set" && IsGetInspectPathComplete(tokens, hasPartialToken))
            {
                return items;
            }

            const size_t dotIndex = currentToken.find('.');
            if (dotIndex != std::string::npos)
            {
                const std::string objectRef(currentToken.substr(0, dotIndex));
                const std::string memberPrefix(currentToken.substr(dotIndex + 1));
                for (const std::string& propertyName :
                     SceneCommandUtils::ListPropertyPathPrefixes(context.ActiveScene, objectRef, memberPrefix))
                {
                    CompletionItem item;
                    item.Label = objectRef + "." + propertyName;
                    item.InsertText = objectRef + "." + propertyName;
                    item.Description = "property";
                    item.Kind = CompletionKind::Property;
                    items.push_back(std::move(item));
                }
                return items;
            }

            if (!currentToken.empty())
            {
                AppendGameObjectCompletions(context, currentToken, items);
            }
            else
            {
                AppendGameObjectCompletions(context, {}, items);
            }

            return items;
        }

        if (commandId == "find")
        {
            if (currentToken.rfind("type=", 0) == 0)
            {
                const std::string_view typePrefix = currentToken.substr(std::string_view("type=").size());
                for (const std::string& typeName : SceneCommandUtils::ListComponentTypeNames(typePrefix))
                {
                    CompletionItem item;
                    item.Label = "type=" + typeName;
                    item.InsertText = "type=" + typeName;
                    item.Description = "component type";
                    item.Kind = CompletionKind::ComponentType;
                    items.push_back(std::move(item));
                }
                return items;
            }

            AppendGameObjectCompletions(context, currentToken, items);
            return items;
        }

        AppendCommandCompletions(currentToken, items);
        return items;
    }
}
