#pragma once

#include "Core.h"

#include <memory>
#include <vector>

namespace minEngine
{
    class IEditorCommand
    {
    public:
        virtual ~IEditorCommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
        virtual const char* GetDescription() const = 0;
    };

    class EditorCommandHistory
    {
    public:
        void Execute(std::unique_ptr<IEditorCommand> command);
        bool Undo();
        bool Redo();
        void Clear();

        bool CanUndo() const { return !m_UndoStack.empty(); }
        bool CanRedo() const { return !m_RedoStack.empty(); }

    private:
        std::vector<std::unique_ptr<IEditorCommand>> m_UndoStack;
        std::vector<std::unique_ptr<IEditorCommand>> m_RedoStack;
    };
}
