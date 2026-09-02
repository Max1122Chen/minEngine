#include "Runtime/Core/Command/BuiltinCommands.h"

#include "Runtime/Core/Command/CommandExecutor.h"
#include "Runtime/Core/Command/CommandRegistry.h"
#include "Runtime/Core/Command/SceneCommandUtils.h"
#include "Runtime/Core/Command/ValidationService.h"
#include "Runtime/Core/PropertyPath/PropertyPath.h"

namespace minEngine::Command
{
    namespace
    {
        CommandResult ExecuteHelp(const CommandContext& context, const std::vector<std::string>& args)
        {
            (void)context;
            (void)args;

            CommandOutputBuilder builder;
            CommandRegistry::Get().ForEach([&builder](const CommandRegistry::StoredCommand& command) {
                if (HasCommandFlag(command.Flags, CommandFlags::Hidden))
                {
                    return;
                }

                builder.AddSegment(CommandOutputKind::ListItemName, command.Id);
                if (!command.Description.empty())
                {
                    builder.AddSegment(CommandOutputKind::Muted, "  ");
                    builder.AddSegment(CommandOutputKind::Muted, command.Description);
                }
                builder.NewLine();
            });

            return builder.BuildOk("OK");
        }

        CommandResult ExecuteGet(const CommandContext& context, const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                CommandOutputBuilder builder;
                builder.AddLine(CommandOutputKind::Error, "Error: get requires a property path.");
                return builder.BuildError("missing property path");
            }

            const std::optional<PropertyPath> propertyPath = PropertyPath::Parse(args.front());
            if (!propertyPath.has_value())
            {
                CommandOutputBuilder builder;
                builder.AddLine(CommandOutputKind::Error, "Error: invalid property path.");
                return builder.BuildError("invalid property path");
            }

            return propertyPath->GetValue(context);
        }

        CommandResult ExecuteSet(const CommandContext& context, const std::vector<std::string>& args)
        {
            if (args.size() < 2)
            {
                CommandOutputBuilder builder;
                builder.AddLine(CommandOutputKind::Error, "Error: set requires <PropertyPath> <value>.");
                return builder.BuildError("missing arguments");
            }

            const std::optional<PropertyPath> propertyPath = PropertyPath::Parse(args.front());
            if (!propertyPath.has_value())
            {
                CommandOutputBuilder builder;
                builder.AddLine(CommandOutputKind::Error, "Error: invalid property path.");
                return builder.BuildError("invalid property path");
            }

            size_t valueTokenIndex = 1;
            if (args.size() > 2 && args[1] == "=")
            {
                valueTokenIndex = 2;
            }

            if (args.size() <= valueTokenIndex)
            {
                CommandOutputBuilder builder;
                builder.AddLine(CommandOutputKind::Error, "Error: set requires a value literal.");
                return builder.BuildError("missing value");
            }

            std::string valueLiteral = args[valueTokenIndex];
            for (size_t index = valueTokenIndex + 1; index < args.size(); ++index)
            {
                valueLiteral.push_back(' ');
                valueLiteral += args[index];
            }

            if (const std::optional<ValidationError> validationError =
                    ValidationService::ValidateSetValue(context, args.front(), valueLiteral))
            {
                return ValidationService::BuildCommandError(*validationError);
            }

            if (context.EditorSetValue)
            {
                return context.EditorSetValue(args.front(), valueLiteral);
            }

            return propertyPath->SetValue(context, valueLiteral);
        }

        CommandResult ExecuteInspect(const CommandContext& context, const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                CommandOutputBuilder builder;
                builder.AddLine(CommandOutputKind::Error, "Error: inspect requires an object reference.");
                return builder.BuildError("missing object reference");
            }

            const std::optional<PropertyPath> propertyPath = PropertyPath::Parse(args.front());
            if (!propertyPath.has_value())
            {
                CommandOutputBuilder builder;
                builder.AddLine(CommandOutputKind::Error, "Error: invalid object reference.");
                return builder.BuildError("invalid object reference");
            }

            return propertyPath->Inspect(context);
        }

        CommandResult ExecuteFind(const CommandContext& context, const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                CommandOutputBuilder builder;
                builder.AddLine(CommandOutputKind::Error, "Error: find requires a query.");
                return builder.BuildError("missing query");
            }

            if (context.ActiveScene == nullptr)
            {
                CommandOutputBuilder builder;
                builder.AddLine(CommandOutputKind::Error, "Error: no active scene.");
                return builder.BuildError("no active scene");
            }

            std::string query = args.front();
            for (size_t index = 1; index < args.size(); ++index)
            {
                query.push_back(' ');
                query += args[index];
            }

            const std::vector<SceneGameObjectMatch> matches =
                SceneCommandUtils::FindGameObjects(context.ActiveScene, query);
            if (matches.empty())
            {
                CommandOutputBuilder builder;
                builder.AddLine(CommandOutputKind::Warning, "No matches.");
                return builder.BuildError("no matches");
            }

            CommandOutputBuilder builder;
            for (const SceneGameObjectMatch& match : matches)
            {
                builder.AddSegment(CommandOutputKind::ListItemName, match.Name);
                builder.AddSegment(CommandOutputKind::Muted, "  ");
                builder.AddSegment(CommandOutputKind::ListItemMeta, match.ClassName);
                if (!match.GuidText.empty())
                {
                    builder.AddSegment(CommandOutputKind::Muted, "  ");
                    builder.AddSegment(CommandOutputKind::Muted, match.GuidText);
                }
                builder.NewLine();
            }

            return builder.BuildOk(std::to_string(matches.size()) + " match(es)");
        }
    }

    void RegisterBuiltinCommands()
    {
        CommandRegistry& registry = CommandRegistry::Get();

        CommandDescriptor helpDescriptor;
        helpDescriptor.Id = "help";
        helpDescriptor.DisplayName = "help";
        helpDescriptor.Description = "List registered commands";
        helpDescriptor.Scope = CommandScope::Both;
        helpDescriptor.Execute = ExecuteHelp;
        registry.Register(std::move(helpDescriptor));

        CommandDescriptor getDescriptor;
        getDescriptor.Id = "get";
        getDescriptor.DisplayName = "get";
        getDescriptor.Description = "Read a property value by path";
        getDescriptor.Scope = CommandScope::Both;
        getDescriptor.Args = {
            CommandArgDescriptor{"PropertyPath", CommandArgType::ObjectRef, true, "Property path"},
        };
        getDescriptor.Execute = ExecuteGet;
        registry.Register(std::move(getDescriptor));

        CommandDescriptor setDescriptor;
        setDescriptor.Id = "set";
        setDescriptor.DisplayName = "set";
        setDescriptor.Description = "Write a primitive property value by path";
        setDescriptor.Scope = CommandScope::Both;
        setDescriptor.Args = {
            CommandArgDescriptor{"PropertyPath", CommandArgType::ObjectRef, true, "Property path"},
            CommandArgDescriptor{"Value", CommandArgType::String, true, "Value literal"},
        };
        setDescriptor.Execute = ExecuteSet;
        registry.Register(std::move(setDescriptor));

        CommandDescriptor inspectDescriptor;
        inspectDescriptor.Id = "inspect";
        inspectDescriptor.DisplayName = "inspect";
        inspectDescriptor.Description = "Inspect an object or nested property";
        inspectDescriptor.Scope = CommandScope::Both;
        inspectDescriptor.Args = {
            CommandArgDescriptor{"ObjectRef", CommandArgType::ObjectRef, true, "Object reference"},
        };
        inspectDescriptor.Execute = ExecuteInspect;
        registry.Register(std::move(inspectDescriptor));

        CommandDescriptor findDescriptor;
        findDescriptor.Id = "find";
        findDescriptor.DisplayName = "find";
        findDescriptor.Description = "Find game objects by name, type=, or name=";
        findDescriptor.Scope = CommandScope::Both;
        findDescriptor.Args = {
            CommandArgDescriptor{"Query", CommandArgType::String, true, "Search query"},
        };
        findDescriptor.Execute = ExecuteFind;
        registry.Register(std::move(findDescriptor));
    }
}
