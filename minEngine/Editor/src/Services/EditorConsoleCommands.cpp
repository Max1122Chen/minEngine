#include "Services/EditorConsoleCommands.h"

#include "Runtime/Core/Command/BuiltinCommands.h"
#include "Runtime/Core/Command/CommandContext.h"
#include "Runtime/Core/Command/CommandRegistry.h"
#include "Runtime/Core/Command/CommandResult.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

namespace minEngine
{
    namespace
    {
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

        s_Registered = true;
    }
}
