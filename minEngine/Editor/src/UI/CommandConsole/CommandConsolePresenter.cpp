#include "UI/CommandConsole/CommandConsolePresenter.h"



#include "Runtime/Core/Command/CompletionService.h"

#include "SubEditor/Scene/SceneEditor.h"



#include "imgui.h"



#include <algorithm>

#include <cstring>

#include <optional>



namespace minEngine

{

    namespace

    {

        constexpr int kMaxVisibleSuggestionLines = 8;



        struct CommandConsoleInputUserData

        {

            CommandConsolePresenter* Presenter = nullptr;

            Command::CommandContext Context;

        };



        int CommandConsoleInputCallback(ImGuiInputTextCallbackData* data)

        {

            auto* userData = static_cast<CommandConsoleInputUserData*>(data->UserData);

            if (userData == nullptr || userData->Presenter == nullptr)

            {

                return 0;

            }



            CommandConsolePresenter& presenter = *userData->Presenter;

            const Command::CommandContext& context = userData->Context;



            if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)

            {

                presenter.AcceptSelectedCompletion(data, context);

                return 0;

            }



            if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)

            {

                if (presenter.IsCompletionOpen())

                {

                    if (data->EventKey == ImGuiKey_UpArrow)

                    {

                        presenter.NavigateCompletionSelection(-1);

                    }

                    else if (data->EventKey == ImGuiKey_DownArrow)

                    {

                        presenter.NavigateCompletionSelection(1);

                    }



                    return 0;

                }



                if (data->EventKey == ImGuiKey_UpArrow)

                {

                    presenter.ApplyHistoryInInput(data, true);

                }

                else if (data->EventKey == ImGuiKey_DownArrow)

                {

                    presenter.ApplyHistoryInInput(data, false);

                }



                return 0;

            }



            if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)

            {

                presenter.OnInputEdited(data, context);

            }



            return 0;

        }

    }



    void CommandConsolePresenter::ClearOutput()

    {

        m_OutputLines.clear();

    }



    void CommandConsolePresenter::PrepareCommandTabFrame(IEditorContext& context)

    {

        m_LastCompletionContext = BuildCommandContext(context);

        UpdateLiveCompletion(m_LastCompletionContext);

    }



    Command::CommandContext CommandConsolePresenter::BuildCommandContext(IEditorContext& context) const

    {

        Command::CommandContext commandContext;

        if (const EditorSubModule* sceneModule = context.FindSubModule(SceneEditor::kModuleId))

        {

            if (const SceneEditor* sceneEditor = dynamic_cast<const SceneEditor*>(sceneModule))

            {

                commandContext.ActiveScene = sceneEditor->GetActiveScene();

            }

        }



        return commandContext;

    }



    void CommandConsolePresenter::AppendInputEcho(std::string_view line)

    {

        if (!m_ShowInputEcho)

        {

            return;

        }



        Command::CommandOutputLine echoLine;

        echoLine.Segments.push_back(Command::CommandOutputSegment{Command::CommandOutputKind::Muted, "> "});

        echoLine.Segments.push_back(

            Command::CommandOutputSegment{Command::CommandOutputKind::InputEcho, std::string(line)});

        m_OutputLines.push_back(std::move(echoLine));

    }



    void CommandConsolePresenter::AppendResult(const Command::CommandResult& result)

    {

        if (!result.Lines.empty())

        {

            m_OutputLines.insert(m_OutputLines.end(), result.Lines.begin(), result.Lines.end());

            return;

        }



        if (!result.Message.empty())

        {

            Command::CommandOutputKind kind = Command::CommandOutputKind::Plain;

            if (result.Status == Command::CommandStatus::Error)

            {

                kind = Command::CommandOutputKind::Error;

            }

            else if (result.Status == Command::CommandStatus::Warning)

            {

                kind = Command::CommandOutputKind::Warning;

            }

            else if (result.Status == Command::CommandStatus::Ok)

            {

                kind = Command::CommandOutputKind::SuccessStatus;

            }



            Command::CommandOutputLine line;

            line.Segments.push_back(Command::CommandOutputSegment{kind, result.Message});

            m_OutputLines.push_back(std::move(line));

        }

    }



    void CommandConsolePresenter::ExecuteInputLine(IEditorContext& context, std::string_view line)

    {

        const std::string trimmedLine(line);

        if (trimmedLine.empty())

        {

            return;

        }



        AppendInputEcho(trimmedLine);

        m_History.Push(trimmedLine);



        const Command::CommandResult result = m_Executor.ExecuteLine(trimmedLine, BuildCommandContext(context));

        AppendResult(result);

        CloseCompletion();

        m_InputMode = ConsoleInputMode::Normal;

        m_History.ResetNavigation();

    }



    size_t CommandConsolePresenter::FindLastTokenStart(std::string_view inputLine)

    {

        if (inputLine.empty())

        {

            return 0;

        }



        const size_t lastSpace = inputLine.find_last_of(" \t");

        if (lastSpace == std::string_view::npos)

        {

            return 0;

        }



        return lastSpace + 1;

    }



    void CommandConsolePresenter::RefreshCompletionCandidates(

        std::string_view inputLine,

        size_t cursorOffset,

        const Command::CommandContext& context,

        bool preserveSelection)

    {

        std::string previousInsertText;

        if (preserveSelection && m_CompletionIndex >= 0

            && m_CompletionIndex < static_cast<int>(m_CompletionCandidates.size()))

        {

            previousInsertText = m_CompletionCandidates[static_cast<size_t>(m_CompletionIndex)].InsertText;

        }



        m_CompletionCandidates = Command::CompletionService::Complete(inputLine, cursorOffset, context);



        if (m_CompletionCandidates.empty())

        {

            m_CompletionIndex = -1;

            if (m_InputMode == ConsoleInputMode::CompletionOpen)

            {

                m_InputMode = ConsoleInputMode::Normal;

            }

            return;

        }



        m_InputMode = ConsoleInputMode::CompletionOpen;



        if (!preserveSelection || previousInsertText.empty())

        {

            m_CompletionIndex = 0;

            return;

        }



        const auto matchedItem = std::find_if(

            m_CompletionCandidates.begin(),

            m_CompletionCandidates.end(),

            [&](const Command::CompletionItem& item) { return item.InsertText == previousInsertText; });

        if (matchedItem != m_CompletionCandidates.end())

        {

            m_CompletionIndex =

                static_cast<int>(std::distance(m_CompletionCandidates.begin(), matchedItem));

        }

        else

        {

            m_CompletionIndex = 0;

        }

    }



    void CommandConsolePresenter::CloseCompletion()

    {

        m_CompletionCandidates.clear();

        m_CompletionIndex = -1;

        if (m_InputMode == ConsoleInputMode::CompletionOpen)

        {

            m_InputMode = ConsoleInputMode::Normal;

        }

    }



    void CommandConsolePresenter::UpdateLiveCompletion(const Command::CommandContext& context)

    {

        if (m_InputMode == ConsoleInputMode::HistoryBrowse)

        {

            return;

        }



        if (m_InputBuffer[0] == '\0')

        {

            CloseCompletion();

            return;

        }



        RefreshCompletionCandidates(m_InputBuffer, std::strlen(m_InputBuffer), context, true);

    }



    void CommandConsolePresenter::ApplyCompletionToInputBuffer(const Command::CompletionItem& item)

    {

        if (item.InsertText.empty())

        {

            return;

        }



        const std::string currentLine(m_InputBuffer);

        const size_t tokenStart = FindLastTokenStart(currentLine);

        const std::string completedLine = currentLine.substr(0, tokenStart) + item.InsertText;

        std::snprintf(m_InputBuffer, sizeof(m_InputBuffer), "%s", completedLine.c_str());

    }



    void CommandConsolePresenter::ApplyCompletionToCallback(

        ImGuiInputTextCallbackData* data,

        const Command::CompletionItem& item)

    {

        if (data == nullptr || item.InsertText.empty())

        {

            return;

        }



        const std::string currentLine(data->Buf, static_cast<size_t>(data->BufTextLen));

        const size_t tokenStart = FindLastTokenStart(currentLine);

        data->DeleteChars(static_cast<int>(tokenStart), data->BufTextLen - static_cast<int>(tokenStart));

        data->InsertChars(static_cast<int>(tokenStart), item.InsertText.c_str());

        data->CursorPos = static_cast<int>(tokenStart + static_cast<int>(item.InsertText.size()));

        data->SelectionStart = data->CursorPos;

        data->SelectionEnd = data->CursorPos;

    }



    void CommandConsolePresenter::OnInputEdited(

        ImGuiInputTextCallbackData* data,

        const Command::CommandContext& context)

    {

        if (m_SuppressNextInputEdit)

        {

            m_SuppressNextInputEdit = false;

            return;

        }



        m_InputMode = ConsoleInputMode::Normal;

        m_History.ResetNavigation();



        const std::string_view editedLine(data->Buf, static_cast<size_t>(data->BufTextLen));

        RefreshCompletionCandidates(editedLine, static_cast<size_t>(data->CursorPos), context, false);

    }



    void CommandConsolePresenter::AcceptSelectedCompletion(

        ImGuiInputTextCallbackData* data,

        const Command::CommandContext& context)

    {

        if (m_CompletionCandidates.empty())

        {

            RefreshCompletionCandidates(

                std::string_view(data->Buf, static_cast<size_t>(data->BufTextLen)),

                static_cast<size_t>(data->CursorPos),

                context,

                false);

        }



        if (m_CompletionCandidates.empty())

        {

            return;

        }



        if (m_CompletionIndex < 0)

        {

            m_CompletionIndex = 0;

        }



        const Command::CompletionItem& selectedItem =

            m_CompletionCandidates[static_cast<size_t>(m_CompletionIndex)];

        m_SuppressNextInputEdit = true;

        ApplyCompletionToCallback(data, selectedItem);



        RefreshCompletionCandidates(

            std::string_view(data->Buf, static_cast<size_t>(data->BufTextLen)),

            static_cast<size_t>(data->CursorPos),

            context,

            true);

        m_InputMode = m_CompletionCandidates.empty() ? ConsoleInputMode::Normal

                                                       : ConsoleInputMode::CompletionOpen;

    }



    void CommandConsolePresenter::NavigateCompletionSelection(int delta)

    {

        if (!IsCompletionOpen() || m_CompletionCandidates.empty())

        {

            return;

        }



        const int candidateCount = static_cast<int>(m_CompletionCandidates.size());

        if (m_CompletionIndex < 0)

        {

            m_CompletionIndex = 0;

            return;

        }



        m_CompletionIndex = (m_CompletionIndex + delta + candidateCount) % candidateCount;

        m_ScrollCompletionSelectionIntoView = true;

    }



    void CommandConsolePresenter::ApplyHistoryInInput(ImGuiInputTextCallbackData* data, bool navigateUp)

    {

        if (data == nullptr)

        {

            return;

        }



        const std::string currentDraft(data->Buf, static_cast<size_t>(data->BufTextLen));

        const std::optional<std::string> historyLine =

            navigateUp ? m_History.NavigateUp(currentDraft) : m_History.NavigateDown(currentDraft);

        if (!historyLine.has_value())

        {

            return;

        }



        CloseCompletion();

        m_InputMode = ConsoleInputMode::HistoryBrowse;

        m_SuppressNextInputEdit = true;



        data->DeleteChars(0, data->BufTextLen);

        data->InsertChars(0, historyLine->c_str());

        data->CursorPos = data->BufTextLen;

        data->SelectionStart = data->CursorPos;

        data->SelectionEnd = data->CursorPos;

    }



    bool CommandConsolePresenter::IsCompletionOpen() const

    {

        return m_InputMode == ConsoleInputMode::CompletionOpen && !m_CompletionCandidates.empty();

    }



    float CommandConsolePresenter::GetSuggestionsBarHeight() const

    {

        if (!IsCompletionOpen())

        {

            return 0.0f;

        }



        const int visibleLineCount = std::min(

            static_cast<int>(m_CompletionCandidates.size()),

            kMaxVisibleSuggestionLines);

        const float lineHeight = ImGui::GetTextLineHeightWithSpacing();

        const ImGuiStyle& imguiStyle = ImGui::GetStyle();

        return static_cast<float>(visibleLineCount) * lineHeight + imguiStyle.WindowPadding.y * 2.0f;

    }



    float CommandConsolePresenter::GetCommandInputRowHeight() const

    {

        return ImGui::GetFrameHeightWithSpacing();

    }



    void CommandConsolePresenter::DrawSuggestionsBar(const CommandConsoleStyle& style, float displayHeight)

    {

        if (!IsCompletionOpen() || displayHeight <= 0.0f)

        {

            return;

        }



        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));

        if (ImGui::BeginChild("CommandConsoleSuggestions", ImVec2(0.0f, displayHeight), true))

        {

            const float availWidth = ImGui::GetContentRegionAvail().x;

            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();

            const int candidateCount = static_cast<int>(m_CompletionCandidates.size());



            ImGuiListClipper clipper;

            clipper.Begin(candidateCount, lineHeight);

            while (clipper.Step())

            {

                for (int itemIndex = clipper.DisplayStart; itemIndex < clipper.DisplayEnd; ++itemIndex)

                {

                    const Command::CompletionItem& item =

                        m_CompletionCandidates[static_cast<size_t>(itemIndex)];

                    const bool selected = itemIndex == m_CompletionIndex;

                    const CommandCompletionRowStyle rowStyle = style.GetCompletionRowStyle(selected);



                    ImGui::PushID(itemIndex);

                    if (ImGui::InvisibleButton("##SuggestionRow", ImVec2(availWidth, lineHeight)))

                    {

                        m_CompletionIndex = itemIndex;

                        m_ScrollCompletionSelectionIntoView = true;

                        m_SuppressNextInputEdit = true;

                        ApplyCompletionToInputBuffer(item);

                        RefreshCompletionCandidates(m_InputBuffer, std::strlen(m_InputBuffer), m_LastCompletionContext, true);

                        m_InputMode = m_CompletionCandidates.empty() ? ConsoleInputMode::Normal

                                                                       : ConsoleInputMode::CompletionOpen;

                    }

                    if (selected && m_ScrollCompletionSelectionIntoView)

                    {

                        ImGui::SetScrollHereY(0.5f);

                    }

                    const ImVec2 rowMin = ImGui::GetItemRectMin();

                    const ImVec2 rowMax = ImGui::GetItemRectMax();

                    ImDrawList* drawList = ImGui::GetWindowDrawList();

                    if (selected)

                    {

                        drawList->AddRectFilled(rowMin, rowMax, rowStyle.SelectionBackground);

                        drawList->AddRectFilled(rowMin, ImVec2(rowMin.x + 2.0f, rowMax.y), rowStyle.SelectionBar);

                    }

                    const ImU32 labelColor = ImGui::ColorConvertFloat4ToU32(rowStyle.LabelColor);

                    const ImU32 descriptionColor = ImGui::ColorConvertFloat4ToU32(rowStyle.DescriptionColor);

                    const float textBaselineY = rowMin.y + (lineHeight - ImGui::GetTextLineHeight()) * 0.5f;

                    const ImVec2 labelPos(rowMin.x + 8.0f, textBaselineY);

                    drawList->AddText(labelPos, labelColor, item.Label.c_str());

                    if (!item.Description.empty())

                    {

                        const float labelWidth = ImGui::CalcTextSize(item.Label.c_str()).x;

                        const std::string descriptionText = "- " + item.Description;

                        drawList->AddText(

                            ImVec2(labelPos.x + labelWidth + 8.0f, textBaselineY),

                            descriptionColor,

                            descriptionText.c_str());

                    }

                    ImGui::PopID();

                }

            }

            clipper.End();

            m_ScrollCompletionSelectionIntoView = false;

        }

        ImGui::EndChild();

        ImGui::PopStyleVar();

    }



    void CommandConsolePresenter::DrawOutputLines(const CommandConsoleStyle& style)

    {

        int rowIndex = 0;

        for (const Command::CommandOutputLine& line : m_OutputLines)

        {

            ImGui::PushID(rowIndex++);



            bool isFirstSegment = true;

            for (const Command::CommandOutputSegment& segment : line.Segments)

            {

                if (!isFirstSegment)

                {

                    ImGui::SameLine(0.0f, 0.0f);

                }



                ImGui::TextColored(style.GetColor(segment.Kind), "%s", segment.Text.c_str());

                isFirstSegment = false;

            }



            ImGui::PopID();

        }

    }



    bool CommandConsolePresenter::DrawInputAndHandleKeys(IEditorContext& context, const CommandConsoleStyle& style)

    {

        (void)style;



        if (m_FocusInputNextFrame)

        {

            ImGui::SetKeyboardFocusHere();

            m_FocusInputNextFrame = false;

        }



        const Command::CommandContext commandContext = BuildCommandContext(context);

        m_LastCompletionContext = commandContext;

        UpdateLiveCompletion(commandContext);



        CommandConsoleInputUserData callbackUserData;

        callbackUserData.Presenter = this;

        callbackUserData.Context = commandContext;



        bool executed = false;

        const bool inputActivated = ImGui::InputText(

            "##CommandConsoleInput",

            m_InputBuffer,

            sizeof(m_InputBuffer),

            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion

                | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackEdit,

            CommandConsoleInputCallback,

            &callbackUserData);



        if (ImGui::IsItemDeactivatedAfterEdit())

        {

            CloseCompletion();

            m_InputMode = ConsoleInputMode::Normal;

            m_History.ResetNavigation();

        }



        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && ImGui::IsItemActive())

        {

            if (IsCompletionOpen())

            {

                CloseCompletion();

            }

            else if (m_InputBuffer[0] != '\0')

            {

                m_InputBuffer[0] = '\0';

                m_History.ResetNavigation();

                CloseCompletion();

                m_InputMode = ConsoleInputMode::Normal;

            }

        }



        if (inputActivated)

        {

            ExecuteInputLine(context, m_InputBuffer);

            m_InputBuffer[0] = '\0';

            executed = true;

        }



        return executed;

    }

}

