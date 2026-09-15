#include "DebugCommand/DebugCommandCompletionService.h"

#include "DebugCommand/DebugCommandRegistry.h"
#include "DebugCommand/DebugCommandSceneUtils.h"
#include "DebugCommand/DebugCommandSetValueValidation.h"
#include "PropertyPath/PropertyPathTypes.h"

namespace minEngine::DebugCommand
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

        void AppendCommandCompletions(
            std::string_view prefix,
            std::string_view sessionTypeId,
            std::vector<CompletionItem>& outItems)
        {
            const std::vector<const DebugCommandRegistry::StoredCommand*> commands =
                DebugCommandRegistry::Get().List(prefix, DebugCommandScope::Both, sessionTypeId);
            for (const DebugCommandRegistry::StoredCommand* command : commands)
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

        bool IsSceneSession(const DebugCommandContext& context)
        {
            return context.ParseContext.ActiveSessionTypeId.empty()
                || context.ParseContext.ActiveSessionTypeId == "Scene";
        }

        void AppendGameObjectCompletions(
            const DebugCommandContext& context,
            std::string_view prefix,
            std::vector<CompletionItem>& outItems)
        {
            if (!IsSceneSession(context))
            {
                return;
            }

            for (const std::string& objectName :
                 DebugCommandSceneUtils::ListGameObjectNames(context.ActiveScene, prefix))
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

    std::vector<CompletionItem> DebugCommandCompletionService::Complete(
        std::string_view line,
        size_t cursorOffset,
        const DebugCommandContext& context)
    {
        (void)cursorOffset;

        const std::string_view sessionTypeId = context.ParseContext.ActiveSessionTypeId;
        std::vector<CompletionItem> items;
        const std::string_view trimmedLine = line;
        if (trimmedLine.empty())
        {
            AppendCommandCompletions({}, sessionTypeId, items);
            return items;
        }

        const std::vector<std::string> tokens = TokenizeLine(trimmedLine);
        const std::string_view currentToken = CurrentToken(trimmedLine);
        const bool hasPartialToken = EndsWithPartialToken(trimmedLine);

        if (tokens.empty() || (tokens.size() == 1 && hasPartialToken))
        {
            AppendCommandCompletions(currentToken, sessionTypeId, items);
            return items;
        }

        const std::string& commandId = tokens.front();
        if (commandId == "get" || commandId == "set" || commandId == "inspect" || commandId == "edit"
            || commandId == "verify")
        {
            if (!IsSceneSession(context))
            {
                // Scene path grammar only for now; Material/AnimGraph use domain verbs.
                return items;
            }

            if ((commandId == "set" || commandId == "edit" || commandId == "verify")
                && IsSetValuePhase(tokens, hasPartialToken))
            {
                const SetValuePhase valuePhase = DebugCommandSetValueValidation::ParseValuePhase(trimmedLine);
                return DebugCommandSetValueValidation::CompleteValue(context, valuePhase);
            }

            if (commandId != "set" && commandId != "edit" && commandId != "verify"
                && IsGetInspectPathComplete(tokens, hasPartialToken))
            {
                return items;
            }

            const size_t dotIndex = currentToken.find('.');
            const std::string_view head =
                dotIndex == std::string::npos ? currentToken : currentToken.substr(0, dotIndex);
            const std::string_view memberPrefix =
                dotIndex == std::string::npos || dotIndex + 1 >= currentToken.size()
                ? std::string_view{}
                : currentToken.substr(dotIndex + 1);

            const size_t atIndex = head.find('@');
            if (atIndex != std::string::npos)
            {
                const std::string_view gameObjectName = head.substr(0, atIndex);
                const std::string_view componentPrefix = head.substr(atIndex + 1);
                if (gameObjectName.empty())
                {
                    return items;
                }

                if (dotIndex != std::string::npos)
                {
                    for (const PropertyPathSuggestion& suggestion : DebugCommandSceneUtils::ListPropertyPathSuggestions(
                             context.ActiveScene,
                             gameObjectName,
                             componentPrefix,
                             memberPrefix))
                    {
                        CompletionItem item;
                        item.Label = suggestion.Label;
                        item.InsertText = suggestion.InsertText;
                        item.Description = suggestion.TypeName;
                        item.Kind = CompletionKind::Property;
                        items.push_back(std::move(item));
                    }
                    return items;
                }

                for (const std::string& componentName : DebugCommandSceneUtils::ListAttachedComponentNames(
                         context.ActiveScene,
                         gameObjectName,
                         componentPrefix))
                {
                    CompletionItem item;
                    const std::string pathHead = std::string(gameObjectName) + "@" + componentName;
                    item.Label = pathHead;
                    item.InsertText = pathHead;
                    item.Description = "component";
                    item.Kind = CompletionKind::ComponentType;
                    items.push_back(std::move(item));
                }
                return items;
            }

            if (dotIndex != std::string::npos)
            {
                for (const PropertyPathSuggestion& suggestion : DebugCommandSceneUtils::ListPropertyPathSuggestions(
                         context.ActiveScene,
                         head,
                         {},
                         memberPrefix))
                {
                    CompletionItem item;
                    item.Label = suggestion.Label;
                    item.InsertText = suggestion.InsertText;
                    item.Description = suggestion.TypeName;
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
            if (!IsSceneSession(context))
            {
                return items;
            }

            if (currentToken.rfind("type=", 0) == 0)
            {
                const std::string_view typePrefix = currentToken.substr(std::string_view("type=").size());
                for (const std::string& typeName : DebugCommandSceneUtils::ListComponentTypeNames(typePrefix))
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

        if (commandId == "rename" || commandId == "delete_go" || commandId == "reparent"
            || commandId == "add_comp" || commandId == "remove_comp" || commandId == "rename_comp"
            || commandId == "move_comp")
        {
            if (!IsSceneSession(context))
            {
                return items;
            }

            if (commandId == "add_comp" && tokens.size() >= 2 && !(tokens.size() == 2 && hasPartialToken))
            {
                const std::string_view typePrefix =
                    (tokens.size() >= 3 && hasPartialToken) ? currentToken : std::string_view{};
                for (const std::string& typeName : DebugCommandSceneUtils::ListComponentTypeNames(typePrefix))
                {
                    CompletionItem item;
                    item.Label = typeName;
                    item.InsertText = typeName;
                    item.Description = "component type";
                    item.Kind = CompletionKind::ComponentType;
                    items.push_back(std::move(item));
                }
                return items;
            }

            if (tokens.size() >= 3 && !hasPartialToken && commandId != "rename_comp" && commandId != "move_comp"
                && commandId != "reparent")
            {
                return items;
            }

            if (commandId == "reparent" && tokens.size() >= 2 && !(tokens.size() == 2 && hasPartialToken))
            {
                CompletionItem rootItem;
                rootItem.Label = "root";
                rootItem.InsertText = "root";
                rootItem.Description = "scene root";
                rootItem.Kind = CompletionKind::ObjectRef;
                items.push_back(std::move(rootItem));
                AppendGameObjectCompletions(context, currentToken, items);
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

        AppendCommandCompletions(currentToken, sessionTypeId, items);
        return items;
    }
}
