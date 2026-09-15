#include "Services/EditorConsoleCommands.h"
#include "Services/EditorAnimGraphDebugCommands.h"
#include "Services/EditorMaterialDebugCommands.h"
#include "Services/EditorSceneDebugCommands.h"

#include "DebugCommand/DebugBuiltinCommands.h"
#include "DebugCommand/DebugCommandContext.h"
#include "DebugCommand/DebugCommandPayloadJson.h"
#include "DebugCommand/DebugCommandRegistry.h"
#include "DebugCommand/DebugCommandResult.h"
#include "PropertyPath/PropertyPath.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/EditorUndoRedoActions.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "Runtime/Core/Serialization/Serializer.h"

#include <optional>

namespace minEngine
{
    namespace
    {
        IEditorContext* GetEditorContext(const DebugCommand::DebugCommandContext& context)
        {
            return static_cast<IEditorContext*>(context.EditorContextOpaque);
        }

        DebugCommand::DebugCommandResult ExecuteListGo(const DebugCommand::DebugCommandContext& context, const std::vector<std::string>& args)
        {
            (void)args;

            if (context.ActiveScene == nullptr)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommand::DebugCommandOutputKind::Error, "Error: no active scene.");
                return builder.BuildError("no active scene");
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            size_t gameObjectCount = 0;
            std::string payloadItems;
            for (const std::shared_ptr<GameObject>& gameObject : context.ActiveScene->GetAllGameObjects())
            {
                if (!gameObject)
                {
                    continue;
                }

                ++gameObjectCount;
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, gameObject->GetName());

                const Reflection::MEClass* gameObjectClass = gameObject->GetClass();
                std::string className;
                if (gameObjectClass != nullptr)
                {
                    className = gameObjectClass->GetName();
                    builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "  ");
                    builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemMeta, className);
                }

                builder.NewLine();

                if (!payloadItems.empty())
                {
                    payloadItems += ',';
                }
                payloadItems += "{\"name\":";
                payloadItems += DebugCommand::DebugCommandPayloadJson::Quote(gameObject->GetName());
                payloadItems += ",\"class\":";
                payloadItems += DebugCommand::DebugCommandPayloadJson::Quote(className);
                payloadItems += ",\"guid\":";
                payloadItems += DebugCommand::DebugCommandPayloadJson::Quote(gameObject->GetGuid().ToString());
                payloadItems += '}';
            }

            std::string payload = "{\"op\":\"list_go\",\"count\":";
            payload += std::to_string(gameObjectCount);
            payload += ",\"items\":[";
            payload += payloadItems;
            payload += "]}";
            builder.SetPayloadJson(std::move(payload));

            return builder.BuildOk(std::to_string(gameObjectCount) + " game object(s)");
        }

        DebugCommand::DebugCommandResult ExecuteUndo(const DebugCommand::DebugCommandContext& context, const std::vector<std::string>& args)
        {
            (void)args;

            IEditorContext* editorContext = GetEditorContext(context);
            if (editorContext == nullptr)
            {
                return BuildUndoDebugCommandResult({});
            }

            return BuildUndoDebugCommandResult(TryUndo(*editorContext));
        }

        DebugCommand::DebugCommandResult ExecuteRedo(const DebugCommand::DebugCommandContext& context, const std::vector<std::string>& args)
        {
            (void)args;

            IEditorContext* editorContext = GetEditorContext(context);
            if (editorContext == nullptr)
            {
                return BuildRedoDebugCommandResult({});
            }

            return BuildRedoDebugCommandResult(TryRedo(*editorContext));
        }

        DebugCommand::DebugCommandResult ExecuteEditorSetValue(
            const DebugCommand::DebugCommandContext& context,
            std::string_view propertyPathText,
            std::string_view valueLiteral)
        {
            const std::optional<DebugCommand::PropertyPath> propertyPath = DebugCommand::PropertyPath::Parse(propertyPathText);
            if (!propertyPath.has_value())
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommand::DebugCommandOutputKind::Error, "Error: invalid property path.");
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
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommand::DebugCommandOutputKind::Error, "Error: no inspecting scene.");
                return builder.BuildError("no inspecting scene");
            }

            if (context.ActiveScene != editorContext->GetInspectingScene())
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: command target scene is not the inspecting scene.");
                return builder.BuildError("not inspecting scene");
            }

            // Play: mutate PIE directly — no editor undo stack / document dirty.
            if (editorContext->IsPlaying())
            {
                return propertyPath->SetValue(context, valueLiteral);
            }

            DebugCommand::PropertySetTransaction transaction;
            DebugCommand::DebugCommandResult buildError;
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

        DebugCommand::DebugCommandResult ExecuteRename(const DebugCommand::DebugCommandContext& context, const std::vector<std::string>& args)
        {
            if (args.size() < 2)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommand::DebugCommandOutputKind::Error, "Error: rename requires <GOName> <NewName>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = GetEditorContext(context);
            if (editorContext == nullptr)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommand::DebugCommandOutputKind::Error, "Error: rename is only available in the editor.");
                return builder.BuildError("editor only");
            }

            SceneEditor* sceneEditor = GetSceneEditor(editorContext);
            if (sceneEditor == nullptr || context.ActiveScene == nullptr)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommand::DebugCommandOutputKind::Error, "Error: no active scene.");
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

                DebugCommand::DebugCommandOutputBuilder builder;
                if (matchCount > 1)
                {
                    builder.AddLine(
                        DebugCommand::DebugCommandOutputKind::Error,
                        "Error: ambiguous game object name '" + args.front() + "'");
                    return builder.BuildError("ambiguous game object");
                }

                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: game object not found '" + args.front() + "'");
                return builder.BuildError("game object not found");
            }

            const std::string oldName = gameObject->GetName();
            if (oldName == newName)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::Path, oldName);
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, " (unchanged)");
                builder.NewLine();
                return builder.BuildOk("unchanged");
            }

            sceneEditor->SubmitRenameGameObject(*editorContext, gameObject->GetID(), newName);

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Path, oldName);
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, " -> ");
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ValueLiteral, newName);
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

        DebugCommand::RegisterBuiltinDebugCommands();
        EditorSceneDebugCommands::Register();
        EditorMaterialDebugCommands::Register();
        EditorAnimGraphDebugCommands::Register();

        DebugCommand::DebugCommandRegistry& registry = DebugCommand::DebugCommandRegistry::Get();

        DebugCommand::DebugCommandDescriptor listGoDescriptor;
        listGoDescriptor.Id = "list_go";
        listGoDescriptor.Domain = "Scene";
        listGoDescriptor.DisplayName = "list_go";
        listGoDescriptor.Description = "List game objects in the active scene";
        listGoDescriptor.Scope = DebugCommand::DebugCommandScope::Editor;
        listGoDescriptor.Execute = ExecuteListGo;
        registry.Register(std::move(listGoDescriptor));

        DebugCommand::DebugCommandDescriptor undoDescriptor;
        undoDescriptor.Id = "undo";
        undoDescriptor.DisplayName = "undo";
        undoDescriptor.Description = "Undo the last editor command";
        undoDescriptor.Scope = DebugCommand::DebugCommandScope::Editor;
        undoDescriptor.Execute = ExecuteUndo;
        registry.Register(std::move(undoDescriptor));

        DebugCommand::DebugCommandDescriptor redoDescriptor;
        redoDescriptor.Id = "redo";
        redoDescriptor.DisplayName = "redo";
        redoDescriptor.Description = "Redo the last undone editor command";
        redoDescriptor.Scope = DebugCommand::DebugCommandScope::Editor;
        redoDescriptor.Execute = ExecuteRedo;
        registry.Register(std::move(redoDescriptor));

        DebugCommand::DebugCommandDescriptor renameDescriptor;
        renameDescriptor.Id = "rename";
        renameDescriptor.Domain = "Scene";
        renameDescriptor.DisplayName = "rename";
        renameDescriptor.Description = "Rename a game object in the active scene";
        renameDescriptor.Scope = DebugCommand::DebugCommandScope::Editor;
        renameDescriptor.Args = {
            DebugCommand::DebugCommandArgDescriptor{"GameObjectName", DebugCommand::DebugCommandArgType::ObjectRef, true, "Current name"},
            DebugCommand::DebugCommandArgDescriptor{"NewName", DebugCommand::DebugCommandArgType::String, true, "New name"},
        };
        renameDescriptor.Execute = ExecuteRename;
        registry.Register(std::move(renameDescriptor));

        s_Registered = true;
    }

    DebugCommand::DebugCommandResult ExecuteEditorConsoleSetValue(
        const DebugCommand::DebugCommandContext& context,
        std::string_view propertyPathText,
        std::string_view valueLiteral)
    {
        return ExecuteEditorSetValue(context, propertyPathText, valueLiteral);
    }

    DebugCommand::DebugCommandResult ExecuteEditorConsoleEditValue(
        const DebugCommand::DebugCommandContext& context,
        std::string_view propertyPathText,
        std::string_view valueLiteral)
    {
        const std::optional<DebugCommand::PropertyPath> propertyPath = DebugCommand::PropertyPath::Parse(propertyPathText);
        if (!propertyPath.has_value())
        {
            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddLine(DebugCommand::DebugCommandOutputKind::Error, "Error: invalid property path.");
            return builder.BuildError("invalid property path");
        }

        IEditorContext* editorContext = GetEditorContext(context);
        if (editorContext == nullptr)
        {
            return propertyPath->SetValue(context, valueLiteral, DebugCommand::PropertyWriteMode::RespectPolicy);
        }

        SceneEditor* sceneEditor = GetSceneEditor(editorContext);
        if (sceneEditor == nullptr || context.ActiveScene == nullptr)
        {
            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddLine(DebugCommand::DebugCommandOutputKind::Error, "Error: no inspecting scene.");
            return builder.BuildError("no inspecting scene");
        }

        if (context.ActiveScene != editorContext->GetInspectingScene())
        {
            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddLine(
                DebugCommand::DebugCommandOutputKind::Error,
                "Error: command target scene is not the inspecting scene.");
            return builder.BuildError("not inspecting scene");
        }

        if (editorContext->IsPlaying())
        {
            return propertyPath->SetValue(context, valueLiteral, DebugCommand::PropertyWriteMode::RespectPolicy);
        }

        DebugCommand::PropertySetTransaction transaction;
        DebugCommand::DebugCommandResult buildError;
        const Serialization::SerializerOptions& serializerOptions = sceneEditor->GetPropertyCommandSerializerOptions();
        if (!propertyPath->TryBuildSetTransaction(
                context,
                valueLiteral,
                transaction,
                buildError,
                &serializerOptions,
                DebugCommand::PropertyWriteMode::RespectPolicy))
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
