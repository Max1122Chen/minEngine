#include "Shell/EditorCommandStack.h"

#include "Shell/EditorSettingsDefaults.h"

namespace minEngine
{
    void EditorCommandStack::SetMaxDepth(uint32_t maxDepth)
    {
        if (maxDepth < kMinMaxUndoStackDepth)
        {
            maxDepth = kMinMaxUndoStackDepth;
        }
        else if (maxDepth > kMaxMaxUndoStackDepth)
        {
            maxDepth = kMaxMaxUndoStackDepth;
        }

        m_MaxDepth = maxDepth;
        TrimUndoStackToMaxDepth();
    }

    void EditorCommandStack::Execute(std::unique_ptr<IEditorCommand> command)
    {
        if (!command)
        {
            return;
        }

        command->Execute();
        m_UndoStack.push_back(std::move(command));
        m_RedoStack.clear();
        TrimUndoStackToMaxDepth();
    }

    bool EditorCommandStack::Undo()
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

    bool EditorCommandStack::Redo()
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

    void EditorCommandStack::Clear()
    {
        m_UndoStack.clear();
        m_RedoStack.clear();
    }

    const char* EditorCommandStack::PeekUndoDescription() const
    {
        if (m_UndoStack.empty())
        {
            return nullptr;
        }

        return m_UndoStack.back()->GetDescription();
    }

    const char* EditorCommandStack::PeekRedoDescription() const
    {
        if (m_RedoStack.empty())
        {
            return nullptr;
        }

        return m_RedoStack.back()->GetDescription();
    }

    void EditorCommandStack::TrimUndoStackToMaxDepth()
    {
        while (m_UndoStack.size() > m_MaxDepth)
        {
            m_UndoStack.erase(m_UndoStack.begin());
        }
    }
}
