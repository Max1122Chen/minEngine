#include "Runtime/Core/Command/BuiltinCommands.h"

#include "Runtime/Core/Command/CommandExecutor.h"
#include "Runtime/Core/Command/CommandRegistry.h"
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

            return propertyPath->SetValue(context, args[1]);
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
        getDescriptor.Execute = ExecuteGet;
        registry.Register(std::move(getDescriptor));

        CommandDescriptor setDescriptor;
        setDescriptor.Id = "set";
        setDescriptor.DisplayName = "set";
        setDescriptor.Description = "Write a primitive property value by path";
        setDescriptor.Scope = CommandScope::Both;
        setDescriptor.Execute = ExecuteSet;
        registry.Register(std::move(setDescriptor));

        CommandDescriptor inspectDescriptor;
        inspectDescriptor.Id = "inspect";
        inspectDescriptor.DisplayName = "inspect";
        inspectDescriptor.Description = "Inspect an object or nested property";
        inspectDescriptor.Scope = CommandScope::Both;
        inspectDescriptor.Execute = ExecuteInspect;
        registry.Register(std::move(inspectDescriptor));
    }
}
