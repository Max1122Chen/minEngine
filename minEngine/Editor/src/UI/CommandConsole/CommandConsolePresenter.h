#pragma once

#include "Runtime/Core/Command/CommandExecutor.h"
#include "Runtime/Core/Command/CommandHistory.h"
#include "Runtime/Core/Command/CommandResult.h"
#include "UI/CommandConsole/CommandConsoleStyle.h"

#include "Shell/IEditorContext.h"

#include <string>
#include <vector>

struct ImGuiInputTextCallbackData;

namespace minEngine
{
    class CommandConsolePresenter
    {
    public:
        void ClearOutput();
        void ExecuteInputLine(IEditorContext& context, std::string_view line);
        void DrawOutputLines(const CommandConsoleStyle& style);
        bool DrawInputAndHandleKeys(IEditorContext& context, const CommandConsoleStyle& style);

        bool GetShowInputEcho() const { return m_ShowInputEcho; }
        void SetShowInputEcho(bool showInputEcho) { m_ShowInputEcho = showInputEcho; }

        void HandleTabCompletion();
        void ApplyHistoryInInput(ImGuiInputTextCallbackData* data, bool navigateUp);

    private:
        Command::CommandExecutor m_Executor;
        Command::CommandHistory m_History;
        std::vector<Command::CommandOutputLine> m_OutputLines;
        char m_InputBuffer[512] = {};
        bool m_ShowInputEcho = true;
        bool m_FocusInputNextFrame = true;
        bool m_ShowCompletionPopup = false;
        int m_CompletionIndex = -1;
        std::vector<std::string> m_CompletionCandidates;

        Command::CommandContext BuildCommandContext(IEditorContext& context) const;
        void AppendResult(const Command::CommandResult& result);
        void AppendInputEcho(std::string_view line);
        void RefreshCompletionCandidates(std::string_view inputLine);
        void ApplyCompletion(std::string_view insertText);
        void DrawCompletionPopup(const CommandConsoleStyle& style);
        static std::string GetCurrentToken(std::string_view inputLine);
    };
}
