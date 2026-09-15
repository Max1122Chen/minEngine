#include "Services/EditorSceneDebugCommands.h"

#include "DebugCommand/DebugCommandContext.h"
#include "DebugCommand/DebugCommandRegistry.h"
#include "DebugCommand/DebugCommandResult.h"
#include "PropertyPath/PropertyPath.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Scene/SceneEditor.h"

#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

namespace minEngine
{
    class EditorSceneDebugCommandsImpl
    {
    public:
        static IEditorContext* GetEditorContext(const DebugCommand::DebugCommandContext& context)
        {
            return static_cast<IEditorContext*>(context.EditorContextOpaque);
        }

        static DebugCommand::DebugCommandResult MissingEditor()
        {
            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddLine(DebugCommand::DebugCommandOutputKind::Error, "Error: this command is only available in the editor.");
            return builder.BuildError("editor only");
        }

        static DebugCommand::DebugCommandResult MissingScene()
        {
            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddLine(DebugCommand::DebugCommandOutputKind::Error, "Error: no active scene.");
            return builder.BuildError("no active scene");
        }

        static bool ResolveEditorScene(
            const DebugCommand::DebugCommandContext& context,
            IEditorContext*& outEditor,
            SceneEditor*& outSceneEditor)
        {
            outEditor = GetEditorContext(context);
            outSceneEditor = nullptr;
            if (outEditor == nullptr)
            {
                return false;
            }

            outSceneEditor = GetSceneEditor(outEditor);
            return outSceneEditor != nullptr && context.ActiveScene != nullptr;
        }

        static GameObject* FindUniqueGameObjectByName(Scene* scene, std::string_view gameObjectName)
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

        static DebugCommand::DebugCommandResult GameObjectLookupError(Scene* scene, std::string_view gameObjectName)
        {
            size_t matchCount = 0;
            if (scene != nullptr)
            {
                for (const std::shared_ptr<GameObject>& candidate : scene->GetAllGameObjects())
                {
                    if (candidate && candidate->GetName() == gameObjectName)
                    {
                        ++matchCount;
                    }
                }
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            if (matchCount > 1)
            {
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: ambiguous game object name '" + std::string(gameObjectName) + "'");
                return builder.BuildError("ambiguous game object");
            }

            builder.AddLine(
                DebugCommand::DebugCommandOutputKind::Error,
                "Error: game object not found '" + std::string(gameObjectName) + "'");
            return builder.BuildError("game object not found");
        }

        static Component* FindUniqueComponent(GameObject& gameObject, std::string_view query)
        {
            Component* matched = nullptr;
            for (const std::shared_ptr<Component>& component : gameObject.GetAllComponents())
            {
                if (!component)
                {
                    continue;
                }

                bool isMatch = component->GetName() == query;
                if (!isMatch)
                {
                    const Reflection::MEClass* componentClass = component->GetClass();
                    if (componentClass != nullptr
                        && DebugCommand::PropertyPath::ComponentTypeMatches(componentClass, query))
                    {
                        isMatch = true;
                    }
                }

                if (!isMatch)
                {
                    continue;
                }

                if (matched != nullptr)
                {
                    return nullptr;
                }

                matched = component.get();
            }

            return matched;
        }

        static DebugCommand::DebugCommandResult ExecuteAddGo(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            IEditorContext* editorContext = nullptr;
            SceneEditor* sceneEditor = nullptr;
            if (!ResolveEditorScene(context, editorContext, sceneEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingScene();
            }

            sceneEditor->SubmitAddEmptyGOToScene(*editorContext);

            if (!args.empty())
            {
                std::string newName = args.front();
                for (size_t index = 1; index < args.size(); ++index)
                {
                    newName.push_back(' ');
                    newName += args[index];
                }

                GameObject* created = context.ActiveScene->FindGameObjectById(
                    sceneEditor->GetSelectedGameObject() != nullptr
                        ? sceneEditor->GetSelectedGameObject()->GetID()
                        : std::numeric_limits<uint64_t>::max());
                if (created != nullptr)
                {
                    sceneEditor->SubmitRenameGameObject(*editorContext, created->GetID(), newName);
                }
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddLine(DebugCommand::DebugCommandOutputKind::SuccessStatus, "Created GameObject");
            return builder.BuildOk("created");
        }

        static DebugCommand::DebugCommandResult ExecuteDeleteGo(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommand::DebugCommandOutputKind::Error, "Error: delete_go requires <GOName>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            SceneEditor* sceneEditor = nullptr;
            if (!ResolveEditorScene(context, editorContext, sceneEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingScene();
            }

            GameObject* gameObject = FindUniqueGameObjectByName(context.ActiveScene, args.front());
            if (gameObject == nullptr)
            {
                return GameObjectLookupError(context.ActiveScene, args.front());
            }

            sceneEditor->SubmitRemoveGameObjectFromScene(*editorContext, gameObject->GetID());

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Path, args.front());
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, " deleted");
            builder.NewLine();
            return builder.BuildOk("deleted");
        }

        static DebugCommand::DebugCommandResult ExecuteAddComp(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.size() < 2)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: add_comp requires <GOName> <ComponentType>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            SceneEditor* sceneEditor = nullptr;
            if (!ResolveEditorScene(context, editorContext, sceneEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingScene();
            }

            GameObject* gameObject = FindUniqueGameObjectByName(context.ActiveScene, args.front());
            if (gameObject == nullptr)
            {
                return GameObjectLookupError(context.ActiveScene, args.front());
            }

            sceneEditor->SubmitAddComponentToGameObject(*editorContext, gameObject->GetID(), args[1]);

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Path, args.front());
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, " += ");
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemMeta, args[1]);
            builder.NewLine();
            return builder.BuildOk("added");
        }

        static DebugCommand::DebugCommandResult ExecuteRemoveComp(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.size() < 2)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: remove_comp requires <GOName> <Component>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            SceneEditor* sceneEditor = nullptr;
            if (!ResolveEditorScene(context, editorContext, sceneEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingScene();
            }

            GameObject* gameObject = FindUniqueGameObjectByName(context.ActiveScene, args.front());
            if (gameObject == nullptr)
            {
                return GameObjectLookupError(context.ActiveScene, args.front());
            }

            Component* component = FindUniqueComponent(*gameObject, args[1]);
            if (component == nullptr)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: component not found or ambiguous '" + args[1] + "'");
                return builder.BuildError("component not found");
            }

            sceneEditor->SubmitRemoveComponentFromGO(*editorContext, *gameObject, *component);

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Path, args.front());
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, " -= ");
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemMeta, args[1]);
            builder.NewLine();
            return builder.BuildOk("removed");
        }

        static DebugCommand::DebugCommandResult ExecuteReparent(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.size() < 2)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: reparent requires <GOName> <ParentName|root>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            SceneEditor* sceneEditor = nullptr;
            if (!ResolveEditorScene(context, editorContext, sceneEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingScene();
            }

            GameObject* gameObject = FindUniqueGameObjectByName(context.ActiveScene, args.front());
            if (gameObject == nullptr)
            {
                return GameObjectLookupError(context.ActiveScene, args.front());
            }

            uint64_t newParentId = SceneEditor::kSceneRootParentId;
            if (args[1] != "root")
            {
                GameObject* newParent = FindUniqueGameObjectByName(context.ActiveScene, args[1]);
                if (newParent == nullptr)
                {
                    return GameObjectLookupError(context.ActiveScene, args[1]);
                }

                newParentId = newParent->GetID();
            }

            sceneEditor->SubmitReparentGameObject(*editorContext, gameObject->GetID(), newParentId);

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Path, args.front());
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, " parent -> ");
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ValueLiteral, args[1]);
            builder.NewLine();
            return builder.BuildOk("reparented");
        }

        static DebugCommand::DebugCommandResult ExecuteRenameComp(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.size() < 3)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: rename_comp requires <GOName> <Component> <NewName>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            SceneEditor* sceneEditor = nullptr;
            if (!ResolveEditorScene(context, editorContext, sceneEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingScene();
            }

            GameObject* gameObject = FindUniqueGameObjectByName(context.ActiveScene, args.front());
            if (gameObject == nullptr)
            {
                return GameObjectLookupError(context.ActiveScene, args.front());
            }

            Component* component = FindUniqueComponent(*gameObject, args[1]);
            if (component == nullptr)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: component not found or ambiguous '" + args[1] + "'");
                return builder.BuildError("component not found");
            }

            sceneEditor->SubmitRenameComponent(
                *editorContext, gameObject->GetID(), component->GetGuid(), args[2]);

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemMeta, args[1]);
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, " -> ");
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ValueLiteral, args[2]);
            builder.NewLine();
            return builder.BuildOk(args[2]);
        }

        static DebugCommand::DebugCommandResult ExecuteMoveComp(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.size() < 3)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: move_comp requires <GOName> <Component> <Index>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            SceneEditor* sceneEditor = nullptr;
            if (!ResolveEditorScene(context, editorContext, sceneEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingScene();
            }

            GameObject* gameObject = FindUniqueGameObjectByName(context.ActiveScene, args.front());
            if (gameObject == nullptr)
            {
                return GameObjectLookupError(context.ActiveScene, args.front());
            }

            Component* component = FindUniqueComponent(*gameObject, args[1]);
            if (component == nullptr)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: component not found or ambiguous '" + args[1] + "'");
                return builder.BuildError("component not found");
            }

            char* parseEnd = nullptr;
            const unsigned long parsedIndex = std::strtoul(args[2].c_str(), &parseEnd, 10);
            if (parseEnd == args[2].c_str() || (parseEnd != nullptr && *parseEnd != '\0'))
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(DebugCommand::DebugCommandOutputKind::Error, "Error: invalid component index.");
                return builder.BuildError("invalid index");
            }

            sceneEditor->SubmitMoveComponent(
                *editorContext, gameObject->GetID(), component->GetGuid(), static_cast<size_t>(parsedIndex));

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemMeta, args[1]);
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, " index -> ");
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ValueLiteral, args[2]);
            builder.NewLine();
            return builder.BuildOk("moved");
        }
    };

    void EditorSceneDebugCommands::Register()
    {
        DebugCommand::DebugCommandRegistry& registry = DebugCommand::DebugCommandRegistry::Get();

        DebugCommand::DebugCommandDescriptor addGo;
        addGo.Id = "add_go";
        addGo.Domain = "Scene";
        addGo.DisplayName = "add_go";
        addGo.Description = "Create an empty GameObject (optional name)";
        addGo.Scope = DebugCommand::DebugCommandScope::Editor;
        addGo.Args = {
            DebugCommand::DebugCommandArgDescriptor{"Name", DebugCommand::DebugCommandArgType::String, false, "Optional name"},
        };
        addGo.Execute = EditorSceneDebugCommandsImpl::ExecuteAddGo;
        registry.Register(std::move(addGo));

        DebugCommand::DebugCommandDescriptor deleteGo;
        deleteGo.Id = "delete_go";
        deleteGo.Domain = "Scene";
        deleteGo.DisplayName = "delete_go";
        deleteGo.Description = "Delete a GameObject by name";
        deleteGo.Scope = DebugCommand::DebugCommandScope::Editor;
        deleteGo.Args = {
            DebugCommand::DebugCommandArgDescriptor{"GameObjectName", DebugCommand::DebugCommandArgType::ObjectRef, true, "Name"},
        };
        deleteGo.Execute = EditorSceneDebugCommandsImpl::ExecuteDeleteGo;
        registry.Register(std::move(deleteGo));

        DebugCommand::DebugCommandDescriptor addComp;
        addComp.Id = "add_comp";
        addComp.Domain = "Scene";
        addComp.DisplayName = "add_comp";
        addComp.Description = "Add a component to a GameObject";
        addComp.Scope = DebugCommand::DebugCommandScope::Editor;
        addComp.Args = {
            DebugCommand::DebugCommandArgDescriptor{"GameObjectName", DebugCommand::DebugCommandArgType::ObjectRef, true, "Owner"},
            DebugCommand::DebugCommandArgDescriptor{"ComponentType", DebugCommand::DebugCommandArgType::String, true, "Type name"},
        };
        addComp.Execute = EditorSceneDebugCommandsImpl::ExecuteAddComp;
        registry.Register(std::move(addComp));

        DebugCommand::DebugCommandDescriptor removeComp;
        removeComp.Id = "remove_comp";
        removeComp.Domain = "Scene";
        removeComp.DisplayName = "remove_comp";
        removeComp.Description = "Remove a component from a GameObject";
        removeComp.Scope = DebugCommand::DebugCommandScope::Editor;
        removeComp.Args = {
            DebugCommand::DebugCommandArgDescriptor{"GameObjectName", DebugCommand::DebugCommandArgType::ObjectRef, true, "Owner"},
            DebugCommand::DebugCommandArgDescriptor{"Component", DebugCommand::DebugCommandArgType::String, true, "Type or instance name"},
        };
        removeComp.Execute = EditorSceneDebugCommandsImpl::ExecuteRemoveComp;
        registry.Register(std::move(removeComp));

        DebugCommand::DebugCommandDescriptor reparent;
        reparent.Id = "reparent";
        reparent.Domain = "Scene";
        reparent.DisplayName = "reparent";
        reparent.Description = "Reparent a GameObject (use root to detach)";
        reparent.Scope = DebugCommand::DebugCommandScope::Editor;
        reparent.Args = {
            DebugCommand::DebugCommandArgDescriptor{"GameObjectName", DebugCommand::DebugCommandArgType::ObjectRef, true, "Child"},
            DebugCommand::DebugCommandArgDescriptor{"ParentName", DebugCommand::DebugCommandArgType::ObjectRef, true, "Parent or root"},
        };
        reparent.Execute = EditorSceneDebugCommandsImpl::ExecuteReparent;
        registry.Register(std::move(reparent));

        DebugCommand::DebugCommandDescriptor renameComp;
        renameComp.Id = "rename_comp";
        renameComp.Domain = "Scene";
        renameComp.DisplayName = "rename_comp";
        renameComp.Description = "Rename a component instance";
        renameComp.Scope = DebugCommand::DebugCommandScope::Editor;
        renameComp.Args = {
            DebugCommand::DebugCommandArgDescriptor{"GameObjectName", DebugCommand::DebugCommandArgType::ObjectRef, true, "Owner"},
            DebugCommand::DebugCommandArgDescriptor{"Component", DebugCommand::DebugCommandArgType::String, true, "Type or instance name"},
            DebugCommand::DebugCommandArgDescriptor{"NewName", DebugCommand::DebugCommandArgType::String, true, "New name"},
        };
        renameComp.Execute = EditorSceneDebugCommandsImpl::ExecuteRenameComp;
        registry.Register(std::move(renameComp));

        DebugCommand::DebugCommandDescriptor moveComp;
        moveComp.Id = "move_comp";
        moveComp.Domain = "Scene";
        moveComp.DisplayName = "move_comp";
        moveComp.Description = "Move a component to a new index";
        moveComp.Scope = DebugCommand::DebugCommandScope::Editor;
        moveComp.Args = {
            DebugCommand::DebugCommandArgDescriptor{"GameObjectName", DebugCommand::DebugCommandArgType::ObjectRef, true, "Owner"},
            DebugCommand::DebugCommandArgDescriptor{"Component", DebugCommand::DebugCommandArgType::String, true, "Type or instance name"},
            DebugCommand::DebugCommandArgDescriptor{"Index", DebugCommand::DebugCommandArgType::Int, true, "New index"},
        };
        moveComp.Execute = EditorSceneDebugCommandsImpl::ExecuteMoveComp;
        registry.Register(std::move(moveComp));
    }
}
