#include "Shell/EditorUndoRedoActions.h"

#include "Shell/EditorCommandStack.h"
#include "Shell/IEditorContext.h"

namespace minEngine
{
    EditorUndoRedoResult TryUndo(IEditorContext& context)
    {
        EditorCommandStack& commandStack = context.GetCommandStack();
        if (!commandStack.CanUndo())
        {
            return {};
        }

        const char* description = commandStack.PeekUndoDescription();
        commandStack.Undo();
        return { true, description };
    }

    EditorUndoRedoResult TryRedo(IEditorContext& context)
    {
        EditorCommandStack& commandStack = context.GetCommandStack();
        if (!commandStack.CanRedo())
        {
            return {};
        }

        const char* description = commandStack.PeekRedoDescription();
        commandStack.Redo();
        return { true, description };
    }

    Command::CommandResult BuildUndoCommandResult(const EditorUndoRedoResult& result)
    {
        if (!result.bPerformed)
        {
            Command::CommandOutputBuilder builder;
            builder.AddLine(Command::CommandOutputKind::Warning, "Nothing to undo");
            Command::CommandResult commandResult = builder.BuildOk("Nothing to undo");
            commandResult.Status = Command::CommandStatus::Warning;
            return commandResult;
        }

        std::string message = "Undid: ";
        if (result.Description != nullptr && result.Description[0] != '\0')
        {
            message += result.Description;
        }
        else
        {
            message += "operation";
        }

        Command::CommandOutputBuilder builder;
        builder.AddLine(Command::CommandOutputKind::SuccessStatus, message);
        return builder.BuildOk(message);
    }

    Command::CommandResult BuildRedoCommandResult(const EditorUndoRedoResult& result)
    {
        if (!result.bPerformed)
        {
            Command::CommandOutputBuilder builder;
            builder.AddLine(Command::CommandOutputKind::Warning, "Nothing to redo");
            Command::CommandResult commandResult = builder.BuildOk("Nothing to redo");
            commandResult.Status = Command::CommandStatus::Warning;
            return commandResult;
        }

        std::string message = "Redid: ";
        if (result.Description != nullptr && result.Description[0] != '\0')
        {
            message += result.Description;
        }
        else
        {
            message += "operation";
        }

        Command::CommandOutputBuilder builder;
        builder.AddLine(Command::CommandOutputKind::SuccessStatus, message);
        return builder.BuildOk(message);
    }
}
