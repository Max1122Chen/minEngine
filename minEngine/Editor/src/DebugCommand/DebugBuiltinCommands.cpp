#include "DebugCommand/DebugBuiltinCommands.h"

#include "DebugCommand/DebugCommandExecutor.h"
#include "DebugCommand/DebugCommandPayloadJson.h"
#include "DebugCommand/DebugCommandRegistry.h"
#include "DebugCommand/DebugCommandSceneUtils.h"
#include "DebugCommand/DebugCommandValidationService.h"
#include "PropertyPath/PropertyPath.h"

namespace minEngine::DebugCommand
{
    namespace
    {
        DebugCommandResult ExecuteHelp(const DebugCommandContext& context, const std::vector<std::string>& args)
        {
            (void)context;
            (void)args;

            DebugCommandOutputBuilder builder;
            DebugCommandRegistry::Get().ForEach([&builder](const DebugCommandRegistry::StoredCommand& command) {
                if (HasDebugCommandFlag(command.Flags, DebugCommandFlags::Hidden))
                {
                    return;
                }

                builder.AddSegment(DebugCommandOutputKind::ListItemName, command.Id);
                if (!command.Description.empty())
                {
                    builder.AddSegment(DebugCommandOutputKind::Muted, "  ");
                    builder.AddSegment(DebugCommandOutputKind::Muted, command.Description);
                }
                builder.NewLine();
            });

            return builder.BuildOk("OK");
        }

        DebugCommandResult ExecuteGet(const DebugCommandContext& context, const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: get requires a property path.");
                return builder.BuildError("missing property path");
            }

            const std::optional<PropertyPath> propertyPath = PropertyPath::Parse(args.front());
            if (!propertyPath.has_value())
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: invalid property path.");
                return builder.BuildError("invalid property path");
            }

            return propertyPath->GetValue(context);
        }

        DebugCommandResult ExecuteSet(const DebugCommandContext& context, const std::vector<std::string>& args)
        {
            if (args.size() < 2)
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: set requires <PropertyPath> <value>.");
                return builder.BuildError("missing arguments");
            }

            const std::optional<PropertyPath> propertyPath = PropertyPath::Parse(args.front());
            if (!propertyPath.has_value())
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: invalid property path.");
                return builder.BuildError("invalid property path");
            }

            size_t valueTokenIndex = 1;
            if (args.size() > 2 && args[1] == "=")
            {
                valueTokenIndex = 2;
            }

            if (args.size() <= valueTokenIndex)
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: set requires a value literal.");
                return builder.BuildError("missing value");
            }

            std::string valueLiteral = args[valueTokenIndex];
            for (size_t index = valueTokenIndex + 1; index < args.size(); ++index)
            {
                valueLiteral.push_back(' ');
                valueLiteral += args[index];
            }

            if (const std::optional<DebugCommandValidationError> validationError =
                    DebugCommandValidationService::ValidateSetValue(context, args.front(), valueLiteral))
            {
                return DebugCommandValidationService::BuildCommandError(*validationError);
            }

            if (context.EditorSetValue)
            {
                return context.EditorSetValue(args.front(), valueLiteral);
            }

            return propertyPath->SetValue(context, valueLiteral);
        }

        DebugCommandResult ExecuteInspect(const DebugCommandContext& context, const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: inspect requires an object reference.");
                return builder.BuildError("missing object reference");
            }

            const std::optional<PropertyPath> propertyPath = PropertyPath::Parse(args.front());
            if (!propertyPath.has_value())
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: invalid object reference.");
                return builder.BuildError("invalid object reference");
            }

            return propertyPath->Inspect(context);
        }

        DebugCommandResult ExecuteFind(const DebugCommandContext& context, const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: find requires a query.");
                return builder.BuildError("missing query");
            }

            if (context.ActiveScene == nullptr)
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: no active scene.");
                return builder.BuildError("no active scene");
            }

            std::string query = args.front();
            for (size_t index = 1; index < args.size(); ++index)
            {
                query.push_back(' ');
                query += args[index];
            }

            const std::vector<DebugCommandSceneGameObjectMatch> matches =
                DebugCommandSceneUtils::FindGameObjects(context.ActiveScene, query);
            if (matches.empty())
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Warning, "No matches.");
                return builder.BuildError("no matches");
            }

            DebugCommandOutputBuilder builder;
            for (const DebugCommandSceneGameObjectMatch& match : matches)
            {
                builder.AddSegment(DebugCommandOutputKind::ListItemName, match.Name);
                builder.AddSegment(DebugCommandOutputKind::Muted, "  ");
                builder.AddSegment(DebugCommandOutputKind::ListItemMeta, match.ClassName);
                if (!match.GuidText.empty())
                {
                    builder.AddSegment(DebugCommandOutputKind::Muted, "  ");
                    builder.AddSegment(DebugCommandOutputKind::Muted, match.GuidText);
                }
                builder.NewLine();
            }

            std::string payload = "{\"op\":\"find\",\"count\":";
            payload += std::to_string(matches.size());
            payload += ",\"items\":[";
            for (size_t index = 0; index < matches.size(); ++index)
            {
                if (index > 0)
                {
                    payload += ',';
                }
                payload += "{\"name\":";
                payload += DebugCommandPayloadJson::Quote(matches[index].Name);
                payload += ",\"class\":";
                payload += DebugCommandPayloadJson::Quote(matches[index].ClassName);
                if (!matches[index].GuidText.empty())
                {
                    payload += ",\"guid\":";
                    payload += DebugCommandPayloadJson::Quote(matches[index].GuidText);
                }
                payload += '}';
            }
            payload += "]}";
            builder.SetPayloadJson(std::move(payload));

            return builder.BuildOk(std::to_string(matches.size()) + " match(es)");
        }
    }

    void RegisterBuiltinDebugCommands()
    {
        DebugCommandRegistry& registry = DebugCommandRegistry::Get();

        DebugCommandDescriptor helpDescriptor;
        helpDescriptor.Id = "help";
        helpDescriptor.DisplayName = "help";
        helpDescriptor.Description = "List registered commands";
        helpDescriptor.Scope = DebugCommandScope::Both;
        helpDescriptor.Execute = ExecuteHelp;
        registry.Register(std::move(helpDescriptor));

        DebugCommandDescriptor getDescriptor;
        getDescriptor.Id = "get";
        getDescriptor.DisplayName = "get";
        getDescriptor.Description = "Read a property value by path";
        getDescriptor.Scope = DebugCommandScope::Both;
        getDescriptor.Args = {
            DebugCommandArgDescriptor{"PropertyPath", DebugCommandArgType::ObjectRef, true, "Property path"},
        };
        getDescriptor.Execute = ExecuteGet;
        registry.Register(std::move(getDescriptor));

        DebugCommandDescriptor setDescriptor;
        setDescriptor.Id = "set";
        setDescriptor.DisplayName = "set";
        setDescriptor.Description = "Write a primitive property value by path";
        setDescriptor.Scope = DebugCommandScope::Both;
        setDescriptor.Args = {
            DebugCommandArgDescriptor{"PropertyPath", DebugCommandArgType::ObjectRef, true, "Property path"},
            DebugCommandArgDescriptor{"Value", DebugCommandArgType::String, true, "Value literal"},
        };
        setDescriptor.Execute = ExecuteSet;
        registry.Register(std::move(setDescriptor));

        DebugCommandDescriptor inspectDescriptor;
        inspectDescriptor.Id = "inspect";
        inspectDescriptor.DisplayName = "inspect";
        inspectDescriptor.Description = "Inspect an object or nested property";
        inspectDescriptor.Scope = DebugCommandScope::Both;
        inspectDescriptor.Args = {
            DebugCommandArgDescriptor{"ObjectRef", DebugCommandArgType::ObjectRef, true, "Object reference"},
        };
        inspectDescriptor.Execute = ExecuteInspect;
        registry.Register(std::move(inspectDescriptor));

        DebugCommandDescriptor findDescriptor;
        findDescriptor.Id = "find";
        findDescriptor.DisplayName = "find";
        findDescriptor.Description = "Find game objects by name, type=, or name=";
        findDescriptor.Scope = DebugCommandScope::Both;
        findDescriptor.Args = {
            DebugCommandArgDescriptor{"Query", DebugCommandArgType::String, true, "Search query"},
        };
        findDescriptor.Execute = ExecuteFind;
        registry.Register(std::move(findDescriptor));
    }
}
