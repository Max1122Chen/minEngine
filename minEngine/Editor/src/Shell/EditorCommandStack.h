#pragma once

#include "Core.h"

#include <cstddef>
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

    class EditorCommandStack
    {
    public:
        void SetMaxDepth(uint32_t maxDepth);
        uint32_t GetMaxDepth() const { return m_MaxDepth; }

        void Execute(std::unique_ptr<IEditorCommand> command);
        bool Undo();
        bool Redo();
        void Clear();

        bool CanUndo() const { return !m_UndoStack.empty(); }
        bool CanRedo() const { return !m_RedoStack.empty(); }

        const char* PeekUndoDescription() const;
        const char* PeekRedoDescription() const;

        size_t GetUndoCount() const { return m_UndoStack.size(); }
        size_t GetRedoCount() const { return m_RedoStack.size(); }

    private:
        void TrimUndoStackToMaxDepth();

        uint32_t m_MaxDepth = 100;
        std::vector<std::unique_ptr<IEditorCommand>> m_UndoStack;
        std::vector<std::unique_ptr<IEditorCommand>> m_RedoStack;
    };
}
