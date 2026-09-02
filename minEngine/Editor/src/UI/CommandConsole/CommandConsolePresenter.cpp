#include "UI/CommandConsole/CommandConsolePresenter.h"

#include "Runtime/Core/Command/CommandRegistry.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <optional>

namespace minEngine
{
    namespace
    {
        struct CommandConsoleInputUserData
        {
            CommandConsolePresenter* Presenter = nullptr;
        };

        int CommandConsoleInputCallback(ImGuiInputTextCallbackData* data)
        {
            auto* userData = static_cast<CommandConsoleInputUserData*>(data->UserData);
            if (userData == nullptr || userData->Presenter == nullptr)
            {
                return 0;
            }

            CommandConsolePresenter& presenter = *userData->Presenter;

            if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
            {
                presenter.HandleTabCompletion();
                return 0;
            }

            if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
            {
                if (data->EventKey == ImGuiKey_UpArrow)
                {
                    userData->Presenter->ApplyHistoryInInput(data, true);
                }
                else if (data->EventKey == ImGuiKey_DownArrow)
                {
                    userData->Presenter->ApplyHistoryInInput(data, false);
                }

                return 0;
            }

            return 0;
        }
    }

    void CommandConsolePresenter::ClearOutput()
    {
        m_OutputLines.clear();
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
        echoLine.Segments.push_back(Command::CommandOutputSegment{Command::CommandOutputKind::InputEcho, std::string(line)});
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
        m_CompletionCandidates.clear();
        m_CompletionIndex = -1;
        m_ShowCompletionPopup = false;
    }

    std::string CommandConsolePresenter::GetCurrentToken(std::string_view inputLine)
    {
        if (inputLine.empty())
        {
            return {};
        }

        const size_t lastSpace = inputLine.find_last_of(" \t");
        if (lastSpace == std::string_view::npos)
        {
            return std::string(inputLine);
        }

        return std::string(inputLine.substr(lastSpace + 1));
    }

    void CommandConsolePresenter::RefreshCompletionCandidates(std::string_view inputLine)
    {
        m_CompletionCandidates.clear();
        m_CompletionIndex = -1;

        const std::string token = GetCurrentToken(inputLine);
        if (token.empty())
        {
            return;
        }

        const std::vector<const Command::CommandRegistry::StoredCommand*> commands =
            Command::CommandRegistry::Get().List(token, Command::CommandScope::Both);
        m_CompletionCandidates.reserve(commands.size());
        for (const Command::CommandRegistry::StoredCommand* command : commands)
        {
            if (command != nullptr)
            {
                m_CompletionCandidates.push_back(command->Id);
            }
        }
    }

    void CommandConsolePresenter::ApplyCompletion(std::string_view insertText)
    {
        if (insertText.empty())
        {
            return;
        }

        std::string currentLine = m_InputBuffer;
        const size_t lastSpace = currentLine.find_last_of(" \t");
        if (lastSpace == std::string::npos)
        {
            currentLine = std::string(insertText);
        }
        else
        {
            currentLine = currentLine.substr(0, lastSpace + 1) + std::string(insertText);
        }

        std::snprintf(m_InputBuffer, sizeof(m_InputBuffer), "%s", currentLine.c_str());
    }

    void CommandConsolePresenter::ApplyHistoryInInput(ImGuiInputTextCallbackData* data, bool navigateUp)
    {
        if (data == nullptr)
        {
            return;
        }

        const std::string currentDraft(data->Buf, data->BufTextLen);
        const std::optional<std::string> historyLine =
            navigateUp ? m_History.NavigateUp(currentDraft) : m_History.NavigateDown(currentDraft);
        if (!historyLine.has_value())
        {
            return;
        }

        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, historyLine->c_str());
    }

    void CommandConsolePresenter::HandleTabCompletion()
    {
        if (m_CompletionCandidates.empty())
        {
            RefreshCompletionCandidates(m_InputBuffer);
        }

        if (m_CompletionCandidates.empty())
        {
            m_ShowCompletionPopup = false;
            return;
        }

        if (m_CompletionIndex < 0)
        {
            m_CompletionIndex = 0;
        }
        else
        {
            m_CompletionIndex = (m_CompletionIndex + 1) % static_cast<int>(m_CompletionCandidates.size());
        }

        ApplyCompletion(m_CompletionCandidates[static_cast<size_t>(m_CompletionIndex)]);
        m_ShowCompletionPopup = true;
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

    void CommandConsolePresenter::DrawCompletionPopup(const CommandConsoleStyle& style)
    {
        if (!m_ShowCompletionPopup || m_CompletionCandidates.empty() || !ImGui::IsItemActive())
        {
            return;
        }

        const ImVec2 anchor = ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y - 4.0f);
        ImGui::SetNextWindowPos(anchor, ImGuiCond_Always, ImVec2(0.0f, 1.0f));
        ImGui::SetNextWindowSizeConstraints(ImVec2(180.0f, 0.0f), ImVec2(480.0f, 200.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
        if (ImGui::Begin(
                "CommandConsoleCompletion",
                nullptr,
                ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs
                    | ImGuiWindowFlags_AlwaysAutoResize))
        {
            for (size_t candidateIndex = 0; candidateIndex < m_CompletionCandidates.size(); ++candidateIndex)
            {
                const bool selected = static_cast<int>(candidateIndex) == m_CompletionIndex;
                const ImVec4 color = selected ? style.GetColor(Command::CommandOutputKind::Plain)
                                              : style.GetColor(Command::CommandOutputKind::Muted);
                ImGui::TextColored(color, "%s", m_CompletionCandidates[candidateIndex].c_str());
            }
            ImGui::End();
        }
        ImGui::PopStyleVar();
    }

    bool CommandConsolePresenter::DrawInputAndHandleKeys(IEditorContext& context, const CommandConsoleStyle& style)
    {
        if (m_FocusInputNextFrame)
        {
            ImGui::SetKeyboardFocusHere();
            m_FocusInputNextFrame = false;
        }

        CommandConsoleInputUserData callbackUserData;
        callbackUserData.Presenter = this;

        bool executed = false;
        const bool inputActivated = ImGui::InputText(
            "##CommandConsoleInput",
            m_InputBuffer,
            sizeof(m_InputBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion
                | ImGuiInputTextFlags_CallbackHistory,
            CommandConsoleInputCallback,
            &callbackUserData);

        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            RefreshCompletionCandidates(m_InputBuffer);
            m_ShowCompletionPopup = false;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            m_ShowCompletionPopup = false;
            m_CompletionIndex = -1;
        }

        DrawCompletionPopup(style);

        if (inputActivated)
        {
            ExecuteInputLine(context, m_InputBuffer);
            m_InputBuffer[0] = '\0';
            m_History.ResetNavigation();
            executed = true;
        }

        return executed;
    }
}
