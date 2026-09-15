#pragma once

#include "DebugCommand/DebugCommandResult.h"

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

    DebugCommand::DebugCommandResult BuildUndoDebugCommandResult(const EditorUndoRedoResult& result);
    DebugCommand::DebugCommandResult BuildRedoDebugCommandResult(const EditorUndoRedoResult& result);
}
