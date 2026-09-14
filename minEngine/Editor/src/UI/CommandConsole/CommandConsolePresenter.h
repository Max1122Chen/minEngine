#pragma once

#include "DebugCommand/DebugCommandExecutor.h"
#include "DebugCommand/DebugCommandHistory.h"
#include "DebugCommand/DebugCommandResult.h"
#include "DebugCommand/DebugCommandCompletionTypes.h"
#include "UI/CommandConsole/CommandConsoleStyle.h"

#include "Shell/IEditorContext.h"

#include <cstdint>
#include <string>
#include <vector>

struct ImGuiInputTextCallbackData;

namespace minEngine
{
    enum class ConsoleInputMode : uint8_t
    {
        Normal,
        CompletionOpen,
        HistoryBrowse,
    };

    class CommandConsolePresenter
    {
    public:
        void ClearOutput();
        void ExecuteInputLine(IEditorContext& context, std::string_view line);
        void DrawOutputLines(const CommandConsoleStyle& style);
        void PrepareCommandTabFrame(IEditorContext& context);
        bool IsCompletionOpen() const;
        float GetSuggestionsBarHeight() const;
        void DrawSuggestionsBar(const CommandConsoleStyle& style, float displayHeight);
        float GetCommandInputRowHeight() const;
        bool DrawInputAndHandleKeys(IEditorContext& context, const CommandConsoleStyle& style);

        bool GetShowInputEcho() const { return m_ShowInputEcho; }
        void SetShowInputEcho(bool showInputEcho) { m_ShowInputEcho = showInputEcho; }

        void OnInputEdited(ImGuiInputTextCallbackData* data, const DebugCommand::DebugCommandContext& context);
        void AcceptSelectedCompletion(ImGuiInputTextCallbackData* data, const DebugCommand::DebugCommandContext& context);
        void NavigateCompletionSelection(int delta);
        void ApplyHistoryInInput(ImGuiInputTextCallbackData* data, bool navigateUp);

    private:
        DebugCommand::DebugCommandExecutor m_Executor;
        DebugCommand::DebugCommandHistory m_History;
        std::vector<DebugCommand::DebugCommandOutputLine> m_OutputLines;
        char m_InputBuffer[512] = {};
        bool m_ShowInputEcho = true;
        bool m_FocusInputNextFrame = true;
        bool m_SuppressNextInputEdit = false;
        bool m_ScrollCompletionSelectionIntoView = false;
        ConsoleInputMode m_InputMode = ConsoleInputMode::Normal;
        int m_CompletionIndex = -1;
        std::vector<DebugCommand::CompletionItem> m_CompletionCandidates;
        DebugCommand::DebugCommandContext m_LastCompletionContext;

        DebugCommand::DebugCommandContext BuildDebugCommandContext(IEditorContext& editorContext) const;
        void AppendResult(const DebugCommand::DebugCommandResult& result);
        void AppendInputEcho(std::string_view line);
        void UpdateLiveCompletion(const DebugCommand::DebugCommandContext& context);
        void RefreshCompletionCandidates(
            std::string_view inputLine,
            size_t cursorOffset,
            const DebugCommand::DebugCommandContext& context,
            bool preserveSelection);
        void CloseCompletion();
        void ApplyCompletionToInputBuffer(const DebugCommand::CompletionItem& item);
        static void ApplyCompletionToCallback(ImGuiInputTextCallbackData* data, const DebugCommand::CompletionItem& item);
        static size_t FindLastTokenStart(std::string_view inputLine);
    };
}
