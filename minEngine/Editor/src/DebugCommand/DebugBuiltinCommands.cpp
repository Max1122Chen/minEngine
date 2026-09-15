#include "DebugCommand/DebugBuiltinCommands.h"

#include "DebugCommand/DebugCommandExecutor.h"
#include "DebugCommand/DebugCommandPayloadJson.h"
#include "DebugCommand/DebugCommandRegistry.h"
#include "DebugCommand/DebugCommandSceneUtils.h"
#include "DebugCommand/DebugCommandValidationService.h"
#include "PropertyPath/PropertyPath.h"

#include <sstream>

namespace minEngine::DebugCommand
{
    namespace
    {
        DebugCommandResult ExecuteHelp(const DebugCommandContext& context, const std::vector<std::string>& args)
        {
            (void)args;

            const std::string_view sessionTypeId = context.ParseContext.ActiveSessionTypeId;
            DebugCommandOutputBuilder builder;
            for (const DebugCommandRegistry::StoredCommand* command :
                 DebugCommandRegistry::Get().List({}, DebugCommandScope::Both, sessionTypeId))
            {
                if (command == nullptr)
                {
                    continue;
                }

                builder.AddSegment(DebugCommandOutputKind::ListItemName, command->Id);
                if (!command->Description.empty())
                {
                    builder.AddSegment(DebugCommandOutputKind::Muted, "  ");
                    builder.AddSegment(DebugCommandOutputKind::Muted, command->Description);
                }
                builder.NewLine();
            }

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

        std::string JoinValueLiteral(const std::vector<std::string>& args, size_t valueTokenIndex)
        {
            std::string valueLiteral = args[valueTokenIndex];
            for (size_t index = valueTokenIndex + 1; index < args.size(); ++index)
            {
                valueLiteral.push_back(' ');
                valueLiteral += args[index];
            }
            return valueLiteral;
        }

        bool TryParseValueTokenIndex(const std::vector<std::string>& args, size_t& outValueTokenIndex)
        {
            if (args.size() < 2)
            {
                return false;
            }

            outValueTokenIndex = 1;
            if (args.size() > 2 && DebugCommandSetValueValidation::IsOptionalAssignmentOperator(args[1]))
            {
                outValueTokenIndex = 2;
            }

            return args.size() > outValueTokenIndex;
        }

        bool ValuesMatchForVerify(std::string_view expected, std::string_view actual)
        {
            if (expected == actual)
            {
                return true;
            }

            double expectedNumber = 0.0;
            double actualNumber = 0.0;
            std::string expectedText(expected);
            std::string actualText(actual);
            std::istringstream expectedStream(expectedText);
            std::istringstream actualStream(actualText);
            if ((expectedStream >> expectedNumber) && expectedStream.eof()
                && (actualStream >> actualNumber) && actualStream.eof())
            {
                return expectedNumber == actualNumber;
            }

            return false;
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

            size_t valueTokenIndex = 0;
            if (!TryParseValueTokenIndex(args, valueTokenIndex))
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: set requires a value literal.");
                return builder.BuildError("missing value");
            }

            const std::string valueLiteral = JoinValueLiteral(args, valueTokenIndex);

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

        DebugCommandResult ExecuteEdit(const DebugCommandContext& context, const std::vector<std::string>& args)
        {
            if (args.size() < 2)
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: edit requires <PropertyPath> <value>.");
                return builder.BuildError("missing arguments");
            }

            const std::optional<PropertyPath> propertyPath = PropertyPath::Parse(args.front());
            if (!propertyPath.has_value())
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: invalid property path.");
                return builder.BuildError("invalid property path");
            }

            size_t valueTokenIndex = 0;
            if (!TryParseValueTokenIndex(args, valueTokenIndex))
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: edit requires a value literal.");
                return builder.BuildError("missing value");
            }

            const std::string valueLiteral = JoinValueLiteral(args, valueTokenIndex);

            if (const std::optional<DebugCommandValidationError> validationError =
                    DebugCommandValidationService::ValidateSetValue(context, args.front(), valueLiteral))
            {
                return DebugCommandValidationService::BuildCommandError(*validationError);
            }

            if (context.EditorEditValue)
            {
                return context.EditorEditValue(args.front(), valueLiteral);
            }

            return propertyPath->SetValue(context, valueLiteral, PropertyWriteMode::RespectPolicy);
        }

        DebugCommandResult ExecuteVerify(const DebugCommandContext& context, const std::vector<std::string>& args)
        {
            if (args.size() < 2)
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: verify requires <PropertyPath> <value>.");
                return builder.BuildError("missing arguments");
            }

            const std::optional<PropertyPath> propertyPath = PropertyPath::Parse(args.front());
            if (!propertyPath.has_value())
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: invalid property path.");
                return builder.BuildError("invalid property path");
            }

            size_t valueTokenIndex = 0;
            if (!TryParseValueTokenIndex(args, valueTokenIndex))
            {
                DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommandOutputKind::Error, "Error: verify requires a value literal.");
                return builder.BuildError("missing value");
            }

            const std::string expectedLiteral = JoinValueLiteral(args, valueTokenIndex);
            const DebugCommandResult getResult = propertyPath->GetValue(context);
            if (getResult.Status != DebugCommandStatus::Ok)
            {
                return getResult;
            }

            const std::string& actualValue = getResult.Message;
            const bool matched = ValuesMatchForVerify(expectedLiteral, actualValue);

            DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommandOutputKind::Path, propertyPath->GetCanonicalPath());
            builder.AddSegment(DebugCommandOutputKind::Muted, matched ? " == " : " != ");
            builder.AddSegment(DebugCommandOutputKind::ValueLiteral, actualValue);
            if (!matched)
            {
                builder.AddSegment(DebugCommandOutputKind::Muted, " (expected ");
                builder.AddSegment(DebugCommandOutputKind::ValueLiteral, expectedLiteral);
                builder.AddSegment(DebugCommandOutputKind::Muted, ")");
            }
            builder.NewLine();

            std::string payload = "{\"op\":\"verify\",\"ok\":";
            payload += matched ? "true" : "false";
            payload += ",\"path\":";
            payload += DebugCommandPayloadJson::Quote(propertyPath->GetCanonicalPath());
            payload += ",\"actual\":";
            payload += DebugCommandPayloadJson::Quote(actualValue);
            payload += ",\"expected\":";
            payload += DebugCommandPayloadJson::Quote(expectedLiteral);
            payload += '}';
            builder.SetPayloadJson(std::move(payload));

            if (matched)
            {
                return builder.BuildOk("ok");
            }

            return builder.BuildError("verify failed");
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
        getDescriptor.Domain = "Scene";
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
        setDescriptor.Domain = "Scene";
        setDescriptor.DisplayName = "set";
        setDescriptor.Description = "Write a primitive property value by path (bypass edit policy)";
        setDescriptor.Scope = DebugCommandScope::Both;
        setDescriptor.Args = {
            DebugCommandArgDescriptor{"PropertyPath", DebugCommandArgType::ObjectRef, true, "Property path"},
            DebugCommandArgDescriptor{"Value", DebugCommandArgType::String, true, "Value literal"},
        };
        setDescriptor.Execute = ExecuteSet;
        registry.Register(std::move(setDescriptor));

        DebugCommandDescriptor editDescriptor;
        editDescriptor.Id = "edit";
        editDescriptor.Domain = "Scene";
        editDescriptor.DisplayName = "edit";
        editDescriptor.Description = "Write a property value respecting editor edit policy";
        editDescriptor.Scope = DebugCommandScope::Both;
        editDescriptor.Args = {
            DebugCommandArgDescriptor{"PropertyPath", DebugCommandArgType::ObjectRef, true, "Property path"},
            DebugCommandArgDescriptor{"Value", DebugCommandArgType::String, true, "Value literal"},
        };
        editDescriptor.Execute = ExecuteEdit;
        registry.Register(std::move(editDescriptor));

        DebugCommandDescriptor verifyDescriptor;
        verifyDescriptor.Id = "verify";
        verifyDescriptor.Domain = "Scene";
        verifyDescriptor.DisplayName = "verify";
        verifyDescriptor.Description = "Assert a property equals a value";
        verifyDescriptor.Scope = DebugCommandScope::Both;
        verifyDescriptor.Args = {
            DebugCommandArgDescriptor{"PropertyPath", DebugCommandArgType::ObjectRef, true, "Property path"},
            DebugCommandArgDescriptor{"Value", DebugCommandArgType::String, true, "Expected value"},
        };
        verifyDescriptor.Execute = ExecuteVerify;
        registry.Register(std::move(verifyDescriptor));

        DebugCommandDescriptor inspectDescriptor;
        inspectDescriptor.Id = "inspect";
        inspectDescriptor.Domain = "Scene";
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
        findDescriptor.Domain = "Scene";
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
