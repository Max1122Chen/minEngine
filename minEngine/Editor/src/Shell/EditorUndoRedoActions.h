#pragma once

#include "Runtime/Core/Command/CommandResult.h"

namespace minEngine
{
    class IEditorContext;

    struct EditorUndoRedoResult
    {
        bool bPerformed = false;
        const char* Description = nullptr;
    };

    EditorUndoRedoResult TryUndo(IEditorContext& context);
    EditorUndoRedoResult TryRedo(IEditorContext& context);

    Command::CommandResult BuildUndoCommandResult(const EditorUndoRedoResult& result);
    Command::CommandResult BuildRedoCommandResult(const EditorUndoRedoResult& result);
}
