#include "Services/EditorAnimGraphDebugCommands.h"

#include "DebugCommand/DebugCommandContext.h"
#include "DebugCommand/DebugCommandPayloadJson.h"
#include "DebugCommand/DebugCommandRegistry.h"
#include "DebugCommand/DebugCommandResult.h"
#include "Runtime/Function/Animation/AnimationGraph.h"
#include "Runtime/Function/Framework/Parameters/ParameterSchema.h"
#include "Runtime/Function/Framework/Parameters/ParameterValueType.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/AnimationGraph/AnimationGraphEditor.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    class EditorAnimGraphDebugCommandsImpl
    {
    public:
        static IEditorContext* GetEditorContext(const DebugCommand::DebugCommandContext& context)
        {
            return static_cast<IEditorContext*>(context.EditorContextOpaque);
        }

        static DebugCommand::DebugCommandResult MissingEditor()
        {
            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddLine(
                DebugCommand::DebugCommandOutputKind::Error,
                "Error: this command is only available in the editor.");
            return builder.BuildError("editor only");
        }

        static DebugCommand::DebugCommandResult MissingGraph()
        {
            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddLine(
                DebugCommand::DebugCommandOutputKind::Error,
                "Error: no open animation graph session.");
            return builder.BuildError("no open anim graph");
        }

        static bool ResolveAnimGraphEditor(
            const DebugCommand::DebugCommandContext& context,
            IEditorContext*& outEditor,
            AnimationGraphEditor*& outAnimGraphEditor)
        {
            outEditor = GetEditorContext(context);
            outAnimGraphEditor = nullptr;
            if (outEditor == nullptr)
            {
                return false;
            }

            outAnimGraphEditor = GetAnimationGraphEditor(outEditor);
            return outAnimGraphEditor != nullptr && outAnimGraphEditor->GetSession().HasOpenGraph();
        }

        static bool EqualsIgnoreCase(std::string_view left, std::string_view right)
        {
            if (left.size() != right.size())
            {
                return false;
            }

            for (size_t i = 0; i < left.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(left[i]))
                    != std::tolower(static_cast<unsigned char>(right[i])))
                {
                    return false;
                }
            }
            return true;
        }

        static bool TryParseIndex(std::string_view text, size_t& outIndex)
        {
            if (text.empty())
            {
                return false;
            }

            char* parseEnd = nullptr;
            const unsigned long value = std::strtoul(std::string(text).c_str(), &parseEnd, 10);
            if (parseEnd == nullptr || *parseEnd != '\0')
            {
                return false;
            }

            outIndex = static_cast<size_t>(value);
            return true;
        }

        static bool TryParseFloat(std::string_view text, float& outValue)
        {
            if (text.empty())
            {
                return false;
            }

            char* parseEnd = nullptr;
            const float value = std::strtof(std::string(text).c_str(), &parseEnd);
            if (parseEnd == nullptr || *parseEnd != '\0')
            {
                return false;
            }

            outValue = value;
            return true;
        }

        static bool TryParseParamType(std::string_view text, ParameterValueType& outType)
        {
            if (EqualsIgnoreCase(text, "Bool") || text == "0")
            {
                outType = ParameterValueType::Bool;
                return true;
            }
            if (EqualsIgnoreCase(text, "Int32") || EqualsIgnoreCase(text, "Int") || text == "1")
            {
                outType = ParameterValueType::Int32;
                return true;
            }
            if (EqualsIgnoreCase(text, "Float") || text == "2")
            {
                outType = ParameterValueType::Float;
                return true;
            }
            return false;
        }

        static const char* ParamTypeName(ParameterValueType type)
        {
            switch (type)
            {
            case ParameterValueType::Bool:
                return "Bool";
            case ParameterValueType::Int32:
                return "Int32";
            case ParameterValueType::Float:
                return "Float";
            }
            return "Unknown";
        }

        static std::vector<uint8_t> PackDefaultBytes(ParameterValueType type)
        {
            std::vector<uint8_t> bytes;
            if (type == ParameterValueType::Bool)
            {
                bytes.push_back(0);
            }
            else if (type == ParameterValueType::Int32)
            {
                const int32_t zero = 0;
                bytes.resize(sizeof(int32_t));
                std::memcpy(bytes.data(), &zero, sizeof(int32_t));
            }
            else
            {
                const float zero = 0.0f;
                bytes.resize(sizeof(float));
                std::memcpy(bytes.data(), &zero, sizeof(float));
            }
            return bytes;
        }

        static DebugCommand::DebugCommandResult ExecuteListStates(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            (void)args;

            IEditorContext* editorContext = nullptr;
            AnimationGraphEditor* animEditor = nullptr;
            if (!ResolveAnimGraphEditor(context, editorContext, animEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingGraph();
            }

            const AnimStateMachine& sm = animEditor->GetSession().GraphAsset->GetStateMachine();
            DebugCommand::DebugCommandOutputBuilder builder;
            std::string payloadItems;
            for (size_t i = 0; i < sm.States.size(); ++i)
            {
                const AnimState& state = sm.States[i];
                const bool isDefault = state.Name == sm.DefaultStateName;
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemMeta, std::to_string(i));
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "  ");
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, state.Name);
                if (isDefault)
                {
                    builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "  [default]");
                }
                builder.NewLine();

                if (!payloadItems.empty())
                {
                    payloadItems += ',';
                }
                payloadItems += "{\"index\":";
                payloadItems += std::to_string(i);
                payloadItems += ",\"name\":";
                payloadItems += DebugCommand::DebugCommandPayloadJson::Quote(state.Name);
                payloadItems += ",\"default\":";
                payloadItems += isDefault ? "true" : "false";
                payloadItems += ",\"x\":";
                payloadItems += std::to_string(state.EditorPosX);
                payloadItems += ",\"y\":";
                payloadItems += std::to_string(state.EditorPosY);
                payloadItems += '}';
            }

            std::string payload = "{\"op\":\"anim_list_states\",\"count\":";
            payload += std::to_string(sm.States.size());
            payload += ",\"default\":";
            payload += DebugCommand::DebugCommandPayloadJson::Quote(sm.DefaultStateName);
            payload += ",\"items\":[";
            payload += payloadItems;
            payload += "]}";
            builder.SetPayloadJson(std::move(payload));
            return builder.BuildOk("ok");
        }

        static DebugCommand::DebugCommandResult ExecuteListTransitions(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            (void)args;

            IEditorContext* editorContext = nullptr;
            AnimationGraphEditor* animEditor = nullptr;
            if (!ResolveAnimGraphEditor(context, editorContext, animEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingGraph();
            }

            const AnimStateMachine& sm = animEditor->GetSession().GraphAsset->GetStateMachine();
            DebugCommand::DebugCommandOutputBuilder builder;
            std::string payloadItems;

            builder.AddLine(DebugCommand::DebugCommandOutputKind::Muted, "Transitions:");
            for (size_t i = 0; i < sm.Transitions.size(); ++i)
            {
                const AnimTransition& transition = sm.Transitions[i];
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemMeta, std::to_string(i));
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "  ");
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, transition.FromStateName);
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, " -> ");
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, transition.ToStateName);
                builder.NewLine();

                if (!payloadItems.empty())
                {
                    payloadItems += ',';
                }
                payloadItems += "{\"kind\":\"transition\",\"index\":";
                payloadItems += std::to_string(i);
                payloadItems += ",\"from\":";
                payloadItems += DebugCommand::DebugCommandPayloadJson::Quote(transition.FromStateName);
                payloadItems += ",\"to\":";
                payloadItems += DebugCommand::DebugCommandPayloadJson::Quote(transition.ToStateName);
                payloadItems += '}';
            }

            builder.AddLine(DebugCommand::DebugCommandOutputKind::Muted, "AnyState:");
            for (size_t i = 0; i < sm.AnyStateTransitions.size(); ++i)
            {
                const AnimTransition& transition = sm.AnyStateTransitions[i];
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemMeta, std::to_string(i));
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "  Any -> ");
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, transition.ToStateName);
                builder.NewLine();

                if (!payloadItems.empty())
                {
                    payloadItems += ',';
                }
                payloadItems += "{\"kind\":\"any\",\"index\":";
                payloadItems += std::to_string(i);
                payloadItems += ",\"to\":";
                payloadItems += DebugCommand::DebugCommandPayloadJson::Quote(transition.ToStateName);
                payloadItems += '}';
            }

            std::string payload = "{\"op\":\"anim_list_transitions\",\"items\":[";
            payload += payloadItems;
            payload += "]}";
            builder.SetPayloadJson(std::move(payload));
            return builder.BuildOk("ok");
        }

        static DebugCommand::DebugCommandResult ExecuteAddState(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            IEditorContext* editorContext = nullptr;
            AnimationGraphEditor* animEditor = nullptr;
            if (!ResolveAnimGraphEditor(context, editorContext, animEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingGraph();
            }
            (void)editorContext;

            float posX = 0.0f;
            float posY = 0.0f;
            if (args.size() >= 2)
            {
                if (!TryParseFloat(args[0], posX) || !TryParseFloat(args[1], posY))
                {
                    DebugCommand::DebugCommandOutputBuilder builder;
                    builder.AddLine(
                        DebugCommand::DebugCommandOutputKind::Error,
                        "Error: invalid position.");
                    return builder.BuildError("invalid position");
                }
            }
            else if (args.size() == 1)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: anim_add_state position requires both x and y.");
                return builder.BuildError("missing y");
            }

            std::string createdName;
            const bool ok = animEditor->SubmitOwnedPropertyMutation(
                "m_StateMachine",
                [&]() { return animEditor->AddStateAt(posX, posY, &createdName); });
            if (!ok)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: failed to add state.");
                return builder.BuildError("add failed");
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, createdName);
            builder.NewLine();
            return builder.BuildOk(createdName);
        }

        static DebugCommand::DebugCommandResult ExecuteRemoveState(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: anim_remove_state requires <StateName>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            AnimationGraphEditor* animEditor = nullptr;
            if (!ResolveAnimGraphEditor(context, editorContext, animEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingGraph();
            }
            (void)editorContext;

            const std::string stateName = args[0];
            const bool ok = animEditor->SubmitOwnedPropertyMutation(
                "m_StateMachine",
                [&]() { return animEditor->RemoveStateByName(stateName); });
            if (!ok)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: state not found or remove failed.");
                return builder.BuildError("remove failed");
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, stateName);
            builder.NewLine();
            return builder.BuildOk("removed");
        }

        static DebugCommand::DebugCommandResult ExecuteRenameState(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.size() < 2)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: anim_rename_state requires <OldName> <NewName>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            AnimationGraphEditor* animEditor = nullptr;
            if (!ResolveAnimGraphEditor(context, editorContext, animEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingGraph();
            }
            (void)editorContext;

            std::string error;
            const bool ok = animEditor->SubmitOwnedPropertyMutation(
                "m_StateMachine",
                [&]() { return animEditor->RenameState(args[0], args[1], &error); });
            if (!ok)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: " + (error.empty() ? std::string("rename failed") : error));
                return builder.BuildError("rename failed");
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, args[0]);
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, " -> ");
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, args[1]);
            builder.NewLine();
            return builder.BuildOk("renamed");
        }

        static DebugCommand::DebugCommandResult ExecuteAddTransition(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.size() < 2)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: anim_add_transition requires <From> <To>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            AnimationGraphEditor* animEditor = nullptr;
            if (!ResolveAnimGraphEditor(context, editorContext, animEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingGraph();
            }
            (void)editorContext;

            std::string error;
            const bool ok = animEditor->SubmitOwnedPropertyMutation(
                "m_StateMachine",
                [&]() { return animEditor->AddTransition(args[0], args[1], &error); });
            if (!ok)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: " + (error.empty() ? std::string("add transition failed") : error));
                return builder.BuildError("add transition failed");
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, args[0]);
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, " -> ");
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, args[1]);
            builder.NewLine();
            return builder.BuildOk("added");
        }

        static DebugCommand::DebugCommandResult ExecuteAddAnyTransition(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: anim_add_any_transition requires <To>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            AnimationGraphEditor* animEditor = nullptr;
            if (!ResolveAnimGraphEditor(context, editorContext, animEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingGraph();
            }
            (void)editorContext;

            std::string error;
            const bool ok = animEditor->SubmitOwnedPropertyMutation(
                "m_StateMachine",
                [&]() { return animEditor->AddAnyStateTransition(args[0], &error); });
            if (!ok)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: " + (error.empty() ? std::string("add any transition failed") : error));
                return builder.BuildError("add any failed");
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "Any -> ");
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, args[0]);
            builder.NewLine();
            return builder.BuildOk("added");
        }

        static DebugCommand::DebugCommandResult ExecuteRemoveTransition(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: anim_remove_transition requires <Index>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            AnimationGraphEditor* animEditor = nullptr;
            if (!ResolveAnimGraphEditor(context, editorContext, animEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingGraph();
            }
            (void)editorContext;

            size_t index = 0;
            if (!TryParseIndex(args[0], index))
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: invalid transition index.");
                return builder.BuildError("invalid index");
            }

            const bool ok = animEditor->SubmitOwnedPropertyMutation(
                "m_StateMachine",
                [&]() { return animEditor->RemoveTransitionAt(index); });
            if (!ok)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: remove transition failed.");
                return builder.BuildError("remove failed");
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(
                DebugCommand::DebugCommandOutputKind::ValueLiteral,
                std::to_string(index));
            builder.NewLine();
            return builder.BuildOk("removed");
        }

        static DebugCommand::DebugCommandResult ExecuteRemoveAnyTransition(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: anim_remove_any_transition requires <Index>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            AnimationGraphEditor* animEditor = nullptr;
            if (!ResolveAnimGraphEditor(context, editorContext, animEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingGraph();
            }
            (void)editorContext;

            size_t index = 0;
            if (!TryParseIndex(args[0], index))
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: invalid any-transition index.");
                return builder.BuildError("invalid index");
            }

            const bool ok = animEditor->SubmitOwnedPropertyMutation(
                "m_StateMachine",
                [&]() { return animEditor->RemoveAnyStateTransitionAt(index); });
            if (!ok)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: remove any transition failed.");
                return builder.BuildError("remove failed");
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(
                DebugCommand::DebugCommandOutputKind::ValueLiteral,
                std::to_string(index));
            builder.NewLine();
            return builder.BuildOk("removed");
        }

        static DebugCommand::DebugCommandResult ExecuteReverseTransition(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: anim_reverse_transition requires <Index>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            AnimationGraphEditor* animEditor = nullptr;
            if (!ResolveAnimGraphEditor(context, editorContext, animEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingGraph();
            }
            (void)editorContext;

            size_t index = 0;
            if (!TryParseIndex(args[0], index))
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: invalid transition index.");
                return builder.BuildError("invalid index");
            }

            const bool ok = animEditor->SubmitOwnedPropertyMutation(
                "m_StateMachine",
                [&]() {
                    AnimGraphSelection selection;
                    selection.Kind = AnimGraphSelectionKind::Transition;
                    selection.TransitionIndex = static_cast<int>(index);
                    animEditor->SetSelection(selection);
                    return animEditor->ReverseTransition();
                });
            if (!ok)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: reverse transition failed.");
                return builder.BuildError("reverse failed");
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(
                DebugCommand::DebugCommandOutputKind::ValueLiteral,
                std::to_string(index));
            builder.NewLine();
            return builder.BuildOk("reversed");
        }

        static DebugCommand::DebugCommandResult ExecuteSetDefault(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: anim_set_default requires <StateName>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            AnimationGraphEditor* animEditor = nullptr;
            if (!ResolveAnimGraphEditor(context, editorContext, animEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingGraph();
            }
            (void)editorContext;

            std::string error;
            const bool ok = animEditor->SubmitOwnedPropertyMutation(
                "m_StateMachine",
                [&]() { return animEditor->SetDefaultStateName(args[0], &error); });
            if (!ok)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: " + (error.empty() ? std::string("set default failed") : error));
                return builder.BuildError("set default failed");
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, args[0]);
            builder.NewLine();
            return builder.BuildOk("ok");
        }

        static DebugCommand::DebugCommandResult ExecuteListParams(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            (void)args;

            IEditorContext* editorContext = nullptr;
            AnimationGraphEditor* animEditor = nullptr;
            if (!ResolveAnimGraphEditor(context, editorContext, animEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingGraph();
            }

            const ParameterSchema& schema = animEditor->GetSession().GraphAsset->GetSchema();
            const std::vector<ParameterSchemaEntry>& entries = schema.GetEntries();
            DebugCommand::DebugCommandOutputBuilder builder;
            std::string payloadItems;
            for (size_t i = 0; i < entries.size(); ++i)
            {
                const ParameterSchemaEntry& entry = entries[i];
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemMeta, std::to_string(i));
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "  ");
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, entry.Name);
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "  ");
                builder.AddSegment(
                    DebugCommand::DebugCommandOutputKind::ListItemMeta,
                    ParamTypeName(entry.Type));
                builder.NewLine();

                if (!payloadItems.empty())
                {
                    payloadItems += ',';
                }
                payloadItems += "{\"index\":";
                payloadItems += std::to_string(i);
                payloadItems += ",\"name\":";
                payloadItems += DebugCommand::DebugCommandPayloadJson::Quote(entry.Name);
                payloadItems += ",\"type\":";
                payloadItems += DebugCommand::DebugCommandPayloadJson::Quote(ParamTypeName(entry.Type));
                payloadItems += '}';
            }

            std::string payload = "{\"op\":\"anim_list_params\",\"count\":";
            payload += std::to_string(entries.size());
            payload += ",\"items\":[";
            payload += payloadItems;
            payload += "]}";
            builder.SetPayloadJson(std::move(payload));
            return builder.BuildOk("ok");
        }

        static DebugCommand::DebugCommandResult ExecuteAddParam(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            IEditorContext* editorContext = nullptr;
            AnimationGraphEditor* animEditor = nullptr;
            if (!ResolveAnimGraphEditor(context, editorContext, animEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingGraph();
            }
            (void)editorContext;

            ParameterValueType type = ParameterValueType::Float;
            std::string requestedName;
            if (!args.empty())
            {
                if (TryParseParamType(args[0], type))
                {
                    if (args.size() >= 2)
                    {
                        requestedName = args[1];
                    }
                }
                else
                {
                    requestedName = args[0];
                    if (args.size() >= 2 && !TryParseParamType(args[1], type))
                    {
                        DebugCommand::DebugCommandOutputBuilder builder;
                        builder.AddLine(
                            DebugCommand::DebugCommandOutputKind::Error,
                            "Error: invalid parameter type.");
                        return builder.BuildError("invalid type");
                    }
                }
            }

            ParameterSchema& schema = animEditor->GetSession().GraphAsset->GetSchema();
            std::string uniqueName = requestedName.empty() ? "Param" : requestedName;
            if (requestedName.empty())
            {
                int suffix = 1;
                while (schema.FindEntryIndex(uniqueName) != SIZE_MAX)
                {
                    uniqueName = "Param_" + std::to_string(suffix++);
                }
            }

            ParameterSchemaEntry entry;
            entry.Name = uniqueName;
            entry.Type = type;
            entry.DefaultBytes = PackDefaultBytes(type);

            std::string error;
            const bool ok = animEditor->SubmitOwnedPropertyMutation(
                "m_Schema",
                [&]() {
                    if (!schema.AddEntry(std::move(entry), &error))
                    {
                        return false;
                    }
                    animEditor->NotifyGraphChanged();
                    return true;
                });
            if (!ok)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: " + (error.empty() ? std::string("add param failed") : error));
                return builder.BuildError("add param failed");
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, uniqueName);
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "  ");
            builder.AddSegment(
                DebugCommand::DebugCommandOutputKind::ListItemMeta,
                ParamTypeName(type));
            builder.NewLine();
            return builder.BuildOk(uniqueName);
        }

        static DebugCommand::DebugCommandResult ExecuteRemoveParam(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: anim_remove_param requires <Name|Index>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            AnimationGraphEditor* animEditor = nullptr;
            if (!ResolveAnimGraphEditor(context, editorContext, animEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingGraph();
            }
            (void)editorContext;

            ParameterSchema& schema = animEditor->GetSession().GraphAsset->GetSchema();
            size_t index = SIZE_MAX;
            if (TryParseIndex(args[0], index))
            {
                if (index >= schema.GetEntries().size())
                {
                    DebugCommand::DebugCommandOutputBuilder builder;
                    builder.AddLine(
                        DebugCommand::DebugCommandOutputKind::Error,
                        "Error: parameter index out of range.");
                    return builder.BuildError("index out of range");
                }
            }
            else
            {
                index = schema.FindEntryIndex(args[0]);
                if (index == SIZE_MAX)
                {
                    DebugCommand::DebugCommandOutputBuilder builder;
                    builder.AddLine(
                        DebugCommand::DebugCommandOutputKind::Error,
                        "Error: parameter not found.");
                    return builder.BuildError("not found");
                }
            }

            const std::string removedName = schema.GetEntries()[index].Name;
            const bool ok = animEditor->SubmitOwnedPropertyMutation(
                "m_Schema",
                [&]() {
                    if (!schema.RemoveEntryAt(index))
                    {
                        return false;
                    }
                    animEditor->NotifyGraphChanged();
                    return true;
                });
            if (!ok)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: remove param failed.");
                return builder.BuildError("remove failed");
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, removedName);
            builder.NewLine();
            return builder.BuildOk("removed");
        }
    };

    void EditorAnimGraphDebugCommands::Register()
    {
        DebugCommand::DebugCommandRegistry& registry = DebugCommand::DebugCommandRegistry::Get();

        DebugCommand::DebugCommandDescriptor listStates;
        listStates.Id = "anim_list_states";
        listStates.Domain = "AnimationGraph";
        listStates.DisplayName = "anim_list_states";
        listStates.Description = "List states in the open animation graph";
        listStates.Scope = DebugCommand::DebugCommandScope::Editor;
        listStates.Execute = EditorAnimGraphDebugCommandsImpl::ExecuteListStates;
        registry.Register(std::move(listStates));

        DebugCommand::DebugCommandDescriptor listTransitions;
        listTransitions.Id = "anim_list_transitions";
        listTransitions.Domain = "AnimationGraph";
        listTransitions.DisplayName = "anim_list_transitions";
        listTransitions.Description = "List transitions (normal + AnyState)";
        listTransitions.Scope = DebugCommand::DebugCommandScope::Editor;
        listTransitions.Execute = EditorAnimGraphDebugCommandsImpl::ExecuteListTransitions;
        registry.Register(std::move(listTransitions));

        DebugCommand::DebugCommandDescriptor addState;
        addState.Id = "anim_add_state";
        addState.Domain = "AnimationGraph";
        addState.DisplayName = "anim_add_state";
        addState.Description = "Add a state via m_StateMachine mutation";
        addState.Scope = DebugCommand::DebugCommandScope::Editor;
        addState.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "X",
                DebugCommand::DebugCommandArgType::Float,
                false,
                "Editor X"},
            DebugCommand::DebugCommandArgDescriptor{
                "Y",
                DebugCommand::DebugCommandArgType::Float,
                false,
                "Editor Y"},
        };
        addState.Execute = EditorAnimGraphDebugCommandsImpl::ExecuteAddState;
        registry.Register(std::move(addState));

        DebugCommand::DebugCommandDescriptor removeState;
        removeState.Id = "anim_remove_state";
        removeState.Domain = "AnimationGraph";
        removeState.DisplayName = "anim_remove_state";
        removeState.Description = "Remove a state by name";
        removeState.Scope = DebugCommand::DebugCommandScope::Editor;
        removeState.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "StateName",
                DebugCommand::DebugCommandArgType::String,
                true,
                "State name"},
        };
        removeState.Execute = EditorAnimGraphDebugCommandsImpl::ExecuteRemoveState;
        registry.Register(std::move(removeState));

        DebugCommand::DebugCommandDescriptor renameState;
        renameState.Id = "anim_rename_state";
        renameState.Domain = "AnimationGraph";
        renameState.DisplayName = "anim_rename_state";
        renameState.Description = "Rename a state";
        renameState.Scope = DebugCommand::DebugCommandScope::Editor;
        renameState.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "OldName",
                DebugCommand::DebugCommandArgType::String,
                true,
                "Current name"},
            DebugCommand::DebugCommandArgDescriptor{
                "NewName",
                DebugCommand::DebugCommandArgType::String,
                true,
                "New name"},
        };
        renameState.Execute = EditorAnimGraphDebugCommandsImpl::ExecuteRenameState;
        registry.Register(std::move(renameState));

        DebugCommand::DebugCommandDescriptor addTransition;
        addTransition.Id = "anim_add_transition";
        addTransition.Domain = "AnimationGraph";
        addTransition.DisplayName = "anim_add_transition";
        addTransition.Description = "Add a state transition";
        addTransition.Scope = DebugCommand::DebugCommandScope::Editor;
        addTransition.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "From",
                DebugCommand::DebugCommandArgType::String,
                true,
                "From state"},
            DebugCommand::DebugCommandArgDescriptor{
                "To",
                DebugCommand::DebugCommandArgType::String,
                true,
                "To state"},
        };
        addTransition.Execute = EditorAnimGraphDebugCommandsImpl::ExecuteAddTransition;
        registry.Register(std::move(addTransition));

        DebugCommand::DebugCommandDescriptor addAny;
        addAny.Id = "anim_add_any_transition";
        addAny.Domain = "AnimationGraph";
        addAny.DisplayName = "anim_add_any_transition";
        addAny.Description = "Add an AnyState transition";
        addAny.Scope = DebugCommand::DebugCommandScope::Editor;
        addAny.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "To",
                DebugCommand::DebugCommandArgType::String,
                true,
                "To state"},
        };
        addAny.Execute = EditorAnimGraphDebugCommandsImpl::ExecuteAddAnyTransition;
        registry.Register(std::move(addAny));

        DebugCommand::DebugCommandDescriptor removeTransition;
        removeTransition.Id = "anim_remove_transition";
        removeTransition.Domain = "AnimationGraph";
        removeTransition.DisplayName = "anim_remove_transition";
        removeTransition.Description = "Remove a transition by index";
        removeTransition.Scope = DebugCommand::DebugCommandScope::Editor;
        removeTransition.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "Index",
                DebugCommand::DebugCommandArgType::Int,
                true,
                "Transition index"},
        };
        removeTransition.Execute = EditorAnimGraphDebugCommandsImpl::ExecuteRemoveTransition;
        registry.Register(std::move(removeTransition));

        DebugCommand::DebugCommandDescriptor removeAny;
        removeAny.Id = "anim_remove_any_transition";
        removeAny.Domain = "AnimationGraph";
        removeAny.DisplayName = "anim_remove_any_transition";
        removeAny.Description = "Remove an AnyState transition by index";
        removeAny.Scope = DebugCommand::DebugCommandScope::Editor;
        removeAny.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "Index",
                DebugCommand::DebugCommandArgType::Int,
                true,
                "AnyState index"},
        };
        removeAny.Execute = EditorAnimGraphDebugCommandsImpl::ExecuteRemoveAnyTransition;
        registry.Register(std::move(removeAny));

        DebugCommand::DebugCommandDescriptor reverse;
        reverse.Id = "anim_reverse_transition";
        reverse.Domain = "AnimationGraph";
        reverse.DisplayName = "anim_reverse_transition";
        reverse.Description = "Reverse a transition by index";
        reverse.Scope = DebugCommand::DebugCommandScope::Editor;
        reverse.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "Index",
                DebugCommand::DebugCommandArgType::Int,
                true,
                "Transition index"},
        };
        reverse.Execute = EditorAnimGraphDebugCommandsImpl::ExecuteReverseTransition;
        registry.Register(std::move(reverse));

        DebugCommand::DebugCommandDescriptor setDefault;
        setDefault.Id = "anim_set_default";
        setDefault.Domain = "AnimationGraph";
        setDefault.DisplayName = "anim_set_default";
        setDefault.Description = "Set the default state";
        setDefault.Scope = DebugCommand::DebugCommandScope::Editor;
        setDefault.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "StateName",
                DebugCommand::DebugCommandArgType::String,
                true,
                "State name"},
        };
        setDefault.Execute = EditorAnimGraphDebugCommandsImpl::ExecuteSetDefault;
        registry.Register(std::move(setDefault));

        DebugCommand::DebugCommandDescriptor listParams;
        listParams.Id = "anim_list_params";
        listParams.Domain = "AnimationGraph";
        listParams.DisplayName = "anim_list_params";
        listParams.Description = "List animation graph schema parameters";
        listParams.Scope = DebugCommand::DebugCommandScope::Editor;
        listParams.Execute = EditorAnimGraphDebugCommandsImpl::ExecuteListParams;
        registry.Register(std::move(listParams));

        DebugCommand::DebugCommandDescriptor addParam;
        addParam.Id = "anim_add_param";
        addParam.Domain = "AnimationGraph";
        addParam.DisplayName = "anim_add_param";
        addParam.Description = "Add a schema parameter ([Type] [Name] or [Name] [Type])";
        addParam.Scope = DebugCommand::DebugCommandScope::Editor;
        addParam.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "Arg0",
                DebugCommand::DebugCommandArgType::String,
                false,
                "Type or Name"},
            DebugCommand::DebugCommandArgDescriptor{
                "Arg1",
                DebugCommand::DebugCommandArgType::String,
                false,
                "Name or Type"},
        };
        addParam.Execute = EditorAnimGraphDebugCommandsImpl::ExecuteAddParam;
        registry.Register(std::move(addParam));

        DebugCommand::DebugCommandDescriptor removeParam;
        removeParam.Id = "anim_remove_param";
        removeParam.Domain = "AnimationGraph";
        removeParam.DisplayName = "anim_remove_param";
        removeParam.Description = "Remove a schema parameter by name or index";
        removeParam.Scope = DebugCommand::DebugCommandScope::Editor;
        removeParam.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "NameOrIndex",
                DebugCommand::DebugCommandArgType::String,
                true,
                "Name or index"},
        };
        removeParam.Execute = EditorAnimGraphDebugCommandsImpl::ExecuteRemoveParam;
        registry.Register(std::move(removeParam));
    }
}
