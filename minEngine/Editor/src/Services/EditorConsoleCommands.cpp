#include "Services/EditorConsoleCommands.h"

#include "Runtime/Core/Command/BuiltinCommands.h"
#include "Runtime/Core/Command/CommandContext.h"
#include "Runtime/Core/Command/CommandRegistry.h"
#include "Runtime/Core/Command/CommandResult.h"
#include "Runtime/Core/PropertyPath/PropertyPath.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/EditorUndoRedoActions.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Scene/SceneEditor.h"

namespace minEngine
{
    namespace
    {
        IEditorContext* GetEditorContext(const Command::CommandContext& context)
        {
            return static_cast<IEditorContext*>(context.EditorContextOpaque);
        }

        Command::CommandResult ExecuteListGo(const Command::CommandContext& context, const std::vector<std::string>& args)
        {
            (void)args;

            if (context.ActiveScene == nullptr)
            {
                Command::CommandOutputBuilder builder;
                builder.AddLine(Command::CommandOutputKind::Error, "Error: no active scene.");
                return builder.BuildError("no active scene");
            }

            Command::CommandOutputBuilder builder;
            size_t gameObjectCount = 0;
            for (const std::shared_ptr<GameObject>& gameObject : context.ActiveScene->GetAllGameObjects())
            {
                if (!gameObject)
                {
                    continue;
                }

                ++gameObjectCount;
                builder.AddSegment(Command::CommandOutputKind::ListItemName, gameObject->GetName());

                const Reflection::MEClass* gameObjectClass = gameObject->GetClass();
                if (gameObjectClass != nullptr)
                {
                    builder.AddSegment(Command::CommandOutputKind::Muted, "  ");
                    builder.AddSegment(Command::CommandOutputKind::ListItemMeta, gameObjectClass->GetName());
                }

                builder.NewLine();
            }

            return builder.BuildOk(std::to_string(gameObjectCount) + " game object(s)");
        }

        Command::CommandResult ExecuteUndo(const Command::CommandContext& context, const std::vector<std::string>& args)
        {
            (void)args;

            IEditorContext* editorContext = GetEditorContext(context);
            if (editorContext == nullptr)
            {
                return BuildUndoCommandResult({});
            }

            return BuildUndoCommandResult(TryUndo(*editorContext));
        }

        Command::CommandResult ExecuteRedo(const Command::CommandContext& context, const std::vector<std::string>& args)
        {
            (void)args;

            IEditorContext* editorContext = GetEditorContext(context);
            if (editorContext == nullptr)
            {
                return BuildRedoCommandResult({});
            }

            return BuildRedoCommandResult(TryRedo(*editorContext));
        }

        Command::CommandResult ExecuteEditorSetValue(
            const Command::CommandContext& context,
            std::string_view propertyPathText,
            std::string_view valueLiteral)
        {
            const std::optional<Command::PropertyPath> propertyPath = Command::PropertyPath::Parse(propertyPathText);
            if (!propertyPath.has_value())
            {
                Command::CommandOutputBuilder builder;
                builder.AddLine(Command::CommandOutputKind::Error, "Error: invalid property path.");
                return builder.BuildError("invalid property path");
            }

            IEditorContext* editorContext = GetEditorContext(context);
            if (editorContext == nullptr)
            {
                return propertyPath->SetValue(context, valueLiteral);
            }

            SceneEditor* sceneEditor = GetSceneEditor(editorContext);
            if (sceneEditor == nullptr || context.ActiveScene == nullptr)
            {
                Command::CommandOutputBuilder builder;
                builder.AddLine(Command::CommandOutputKind::Error, "Error: no active scene.");
                return builder.BuildError("no active scene");
            }

            Command::PropertySetTransaction transaction;
            Command::CommandResult buildError;
            const Serialization::SerializerOptions& serializerOptions = sceneEditor->GetPropertyCommandSerializerOptions();
            if (!propertyPath->TryBuildSetTransaction(
                    context,
                    valueLiteral,
                    transaction,
                    buildError,
                    &serializerOptions))
            {
                return buildError;
            }

            if (transaction.BeforeValue == transaction.AfterValue)
            {
                return propertyPath->BuildSetValueSuccessResult(context);
            }

            sceneEditor->SubmitSetObjectProperty(
                *editorContext,
                transaction.OwnerGuid,
                transaction.OwnerClassName,
                transaction.PropertySubPath,
                std::move(transaction.BeforeValue),
                std::move(transaction.AfterValue));

            return propertyPath->BuildSetValueSuccessResult(context);
        }
    }

    void RegisterEditorConsoleCommands()
    {
        static bool s_Registered = false;
        if (s_Registered)
        {
            return;
        }

        Command::RegisterBuiltinCommands();

        Command::CommandRegistry& registry = Command::CommandRegistry::Get();

        Command::CommandDescriptor listGoDescriptor;
        listGoDescriptor.Id = "list_go";
        listGoDescriptor.DisplayName = "list_go";
        listGoDescriptor.Description = "List game objects in the active scene";
        listGoDescriptor.Scope = Command::CommandScope::Editor;
        listGoDescriptor.Execute = ExecuteListGo;
        registry.Register(std::move(listGoDescriptor));

        Command::CommandDescriptor undoDescriptor;
        undoDescriptor.Id = "undo";
        undoDescriptor.DisplayName = "undo";
        undoDescriptor.Description = "Undo the last editor command";
        undoDescriptor.Scope = Command::CommandScope::Editor;
        undoDescriptor.Execute = ExecuteUndo;
        registry.Register(std::move(undoDescriptor));

        Command::CommandDescriptor redoDescriptor;
        redoDescriptor.Id = "redo";
        redoDescriptor.DisplayName = "redo";
        redoDescriptor.Description = "Redo the last undone editor command";
        redoDescriptor.Scope = Command::CommandScope::Editor;
        redoDescriptor.Execute = ExecuteRedo;
        registry.Register(std::move(redoDescriptor));

        s_Registered = true;
    }

    Command::CommandResult ExecuteEditorConsoleSetValue(
        const Command::CommandContext& context,
        std::string_view propertyPathText,
        std::string_view valueLiteral)
    {
        return ExecuteEditorSetValue(context, propertyPathText, valueLiteral);
    }
}
