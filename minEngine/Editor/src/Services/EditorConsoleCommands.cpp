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
                builder.AddLine(Command::CommandOutputKind::Error, "Error: no inspecting scene.");
                return builder.BuildError("no inspecting scene");
            }

            if (context.ActiveScene != editorContext->GetInspectingScene())
            {
                Command::CommandOutputBuilder builder;
                builder.AddLine(
                    Command::CommandOutputKind::Error,
                    "Error: command target scene is not the inspecting scene.");
                return builder.BuildError("not inspecting scene");
            }

            // Play: mutate PIE directly — no editor undo stack / document dirty.
            if (editorContext->IsPlaying())
            {
                return propertyPath->SetValue(context, valueLiteral);
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

        GameObject* FindUniqueGameObjectByName(Scene* scene, std::string_view gameObjectName)
        {
            if (scene == nullptr || gameObjectName.empty())
            {
                return nullptr;
            }

            GameObject* matchedGameObject = nullptr;
            for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
            {
                if (!gameObject || gameObject->GetName() != gameObjectName)
                {
                    continue;
                }

                if (matchedGameObject != nullptr)
                {
                    return nullptr;
                }

                matchedGameObject = gameObject.get();
            }

            return matchedGameObject;
        }

        Command::CommandResult ExecuteRename(const Command::CommandContext& context, const std::vector<std::string>& args)
        {
            if (args.size() < 2)
            {
                Command::CommandOutputBuilder builder;
                builder.AddLine(Command::CommandOutputKind::Error, "Error: rename requires <GOName> <NewName>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = GetEditorContext(context);
            if (editorContext == nullptr)
            {
                Command::CommandOutputBuilder builder;
                builder.AddLine(Command::CommandOutputKind::Error, "Error: rename is only available in the editor.");
                return builder.BuildError("editor only");
            }

            SceneEditor* sceneEditor = GetSceneEditor(editorContext);
            if (sceneEditor == nullptr || context.ActiveScene == nullptr)
            {
                Command::CommandOutputBuilder builder;
                builder.AddLine(Command::CommandOutputKind::Error, "Error: no active scene.");
                return builder.BuildError("no active scene");
            }

            std::string newName = args[1];
            for (size_t index = 2; index < args.size(); ++index)
            {
                newName.push_back(' ');
                newName += args[index];
            }

            GameObject* gameObject = FindUniqueGameObjectByName(context.ActiveScene, args.front());
            if (gameObject == nullptr)
            {
                size_t matchCount = 0;
                for (const std::shared_ptr<GameObject>& candidate : context.ActiveScene->GetAllGameObjects())
                {
                    if (candidate && candidate->GetName() == args.front())
                    {
                        ++matchCount;
                    }
                }

                Command::CommandOutputBuilder builder;
                if (matchCount > 1)
                {
                    builder.AddLine(
                        Command::CommandOutputKind::Error,
                        "Error: ambiguous game object name '" + args.front() + "'");
                    return builder.BuildError("ambiguous game object");
                }

                builder.AddLine(
                    Command::CommandOutputKind::Error,
                    "Error: game object not found '" + args.front() + "'");
                return builder.BuildError("game object not found");
            }

            const std::string oldName = gameObject->GetName();
            if (oldName == newName)
            {
                Command::CommandOutputBuilder builder;
                builder.AddSegment(Command::CommandOutputKind::Path, oldName);
                builder.AddSegment(Command::CommandOutputKind::Muted, " (unchanged)");
                builder.NewLine();
                return builder.BuildOk("unchanged");
            }

            sceneEditor->SubmitRenameGameObject(*editorContext, gameObject->GetID(), newName);

            Command::CommandOutputBuilder builder;
            builder.AddSegment(Command::CommandOutputKind::Path, oldName);
            builder.AddSegment(Command::CommandOutputKind::Muted, " -> ");
            builder.AddSegment(Command::CommandOutputKind::ValueLiteral, newName);
            builder.NewLine();
            return builder.BuildOk(newName);
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

        Command::CommandDescriptor renameDescriptor;
        renameDescriptor.Id = "rename";
        renameDescriptor.DisplayName = "rename";
        renameDescriptor.Description = "Rename a game object in the active scene";
        renameDescriptor.Scope = Command::CommandScope::Editor;
        renameDescriptor.Args = {
            Command::CommandArgDescriptor{"GameObjectName", Command::CommandArgType::ObjectRef, true, "Current name"},
            Command::CommandArgDescriptor{"NewName", Command::CommandArgType::String, true, "New name"},
        };
        renameDescriptor.Execute = ExecuteRename;
        registry.Register(std::move(renameDescriptor));

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
