#include "Shell/EditorCommandHistory.h"

namespace minEngine
{
    void EditorCommandHistory::Execute(std::unique_ptr<IEditorCommand> command)
    {
        if (!command)
        {
            return;
        }

        command->Execute();
        m_UndoStack.push_back(std::move(command));
        m_RedoStack.clear();
    }

    bool EditorCommandHistory::Undo()
    {
        if (m_UndoStack.empty())
        {
            return false;
        }

        std::unique_ptr<IEditorCommand> command = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();
        command->Undo();
        m_RedoStack.push_back(std::move(command));
        return true;
    }

    bool EditorCommandHistory::Redo()
    {
        if (m_RedoStack.empty())
        {
            return false;
        }

        std::unique_ptr<IEditorCommand> command = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();
        command->Execute();
        m_UndoStack.push_back(std::move(command));
        return true;
    }

    void EditorCommandHistory::Clear()
    {
        m_UndoStack.clear();
        m_RedoStack.clear();
    }
}
