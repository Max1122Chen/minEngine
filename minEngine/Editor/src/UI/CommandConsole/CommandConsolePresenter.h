#pragma once



#include "Runtime/Core/Command/CommandExecutor.h"

#include "Runtime/Core/Command/CommandHistory.h"

#include "Runtime/Core/Command/CommandResult.h"

#include "Runtime/Core/Command/CompletionTypes.h"

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



        void OnInputEdited(ImGuiInputTextCallbackData* data, const Command::CommandContext& context);

        void AcceptSelectedCompletion(ImGuiInputTextCallbackData* data, const Command::CommandContext& context);

        void NavigateCompletionSelection(int delta);

        void ApplyHistoryInInput(ImGuiInputTextCallbackData* data, bool navigateUp);



    private:

        Command::CommandExecutor m_Executor;

        Command::CommandHistory m_History;

        std::vector<Command::CommandOutputLine> m_OutputLines;

        char m_InputBuffer[512] = {};

        bool m_ShowInputEcho = true;

        bool m_FocusInputNextFrame = true;

        bool m_SuppressNextInputEdit = false;

        bool m_ScrollCompletionSelectionIntoView = false;

        ConsoleInputMode m_InputMode = ConsoleInputMode::Normal;

        int m_CompletionIndex = -1;

        std::vector<Command::CompletionItem> m_CompletionCandidates;

        Command::CommandContext m_LastCompletionContext;



        Command::CommandContext BuildCommandContext(IEditorContext& editorContext) const;

        void AppendResult(const Command::CommandResult& result);

        void AppendInputEcho(std::string_view line);

        void UpdateLiveCompletion(const Command::CommandContext& context);

        void RefreshCompletionCandidates(

            std::string_view inputLine,

            size_t cursorOffset,

            const Command::CommandContext& context,

            bool preserveSelection);

        void CloseCompletion();

        void ApplyCompletionToInputBuffer(const Command::CompletionItem& item);

        static void ApplyCompletionToCallback(ImGuiInputTextCallbackData* data, const Command::CompletionItem& item);

        static size_t FindLastTokenStart(std::string_view inputLine);

    };

}

