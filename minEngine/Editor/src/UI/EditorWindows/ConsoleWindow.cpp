#include "UI/EditorWindows/ConsoleWindow.h"

#include "UI/CommandConsole/CommandConsoleStyle.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace minEngine
{
    namespace
    {
        void CommandConsoleSizeConstraint(ImGuiSizeCallbackData* data)
        {
            if (data == nullptr || data->UserData == nullptr)
            {
                return;
            }

            const float minHeight = *static_cast<const float*>(data->UserData);
            if (data->DesiredSize.y < minHeight)
            {
                data->DesiredSize.y = minHeight;
            }
        }
    }

    ConsoleWindow::ConsoleWindow(IEditorContext& context)
        : EditorWindow(context)
    {
    }

    void ConsoleWindow::OnDraw()
    {
        const bool isPlaying = m_Context.IsPlaying();
        m_LastIsPlaying = isPlaying;

        if (m_ActiveTab == ConsoleTab::Command)
        {
            m_CommandModeMinWindowHeight = GetCommandModeMinWindowHeight();
            ImGui::SetNextWindowSizeConstraints(
                ImVec2(-1.0f, m_CommandModeMinWindowHeight),
                ImVec2(-1.0f, FLT_MAX),
                CommandConsoleSizeConstraint,
                &m_CommandModeMinWindowHeight);
        }

        if (!EditorWindowTypography::BeginPanel(
                m_Context,
                m_Title.c_str(),
                nullptr,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            return;
        }

        EditorTypographyScope bodyTypography(m_Context.GetEditorAppearance(), EditorTypographyRole::Body);

        if (ImGui::BeginTabBar("ConsoleTabs"))
        {
            if (ImGui::BeginTabItem("Output"))
            {
                m_ActiveTab = ConsoleTab::Output;
                DrawOutputTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Command"))
            {
                m_ActiveTab = ConsoleTab::Command;
                DrawCommandTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    float ConsoleWindow::GetCommandModeMinWindowHeight() const
    {
        const ImGuiStyle& imguiStyle = ImGui::GetStyle();
        const float tabBarHeight = ImGui::GetTextLineHeightWithSpacing() + imguiStyle.FramePadding.y * 2.0f;
        const float toolbarHeight = ImGui::GetFrameHeightWithSpacing() + imguiStyle.FramePadding.y * 2.0f;
        const float separatorChrome = imguiStyle.ItemSpacing.y + 1.0f;
        const float inputChrome = m_CommandPresenter.GetCommandInputRowHeight() + separatorChrome;
        constexpr float kMinScrollHeight = 40.0f;
        const float windowPadding = imguiStyle.WindowPadding.y * 2.0f;

        return windowPadding + tabBarHeight + toolbarHeight + separatorChrome + kMinScrollHeight + inputChrome
            + separatorChrome;
    }

    void ConsoleWindow::DrawOutputTab()
    {
        bool requestCopyVisible = false;

        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
            if (ImGui::Button("Clear"))
            {
                LogConsoleStorage::Clear();
                m_PausedEntries.clear();
                m_HasPausedSnapshot = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Copy"))
            {
                requestCopyVisible = true;
            }
            ImGui::SameLine();
            ImGui::Checkbox("AutoScroll", &m_AutoScroll);
            ImGui::SameLine();
            ImGui::Checkbox("Pause", &m_PauseStream);
            ImGui::PopStyleVar();
        }

        ImGui::Separator();
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
            std::vector<UI::FilterSection> filterSections = {
                {
                    "Source",
                    {
                        {"Core", &m_ShowCore},
                        {"App", &m_ShowClient},
                    },
                },
                {
                    "Level",
                    {
                        {"Trace", &m_ShowTrace},
                        {"Debug", &m_ShowDebug},
                        {"Info", &m_ShowInfo},
                        {"Warn", &m_ShowWarn},
                        {"Error", &m_ShowError},
                        {"Fatal", &m_ShowFatal},
                    },
                },
            };

            ImGui::SetNextItemWidth(180.0f);
            UI::DrawFilterDropdown("##ConsoleFilterCombo", filterSections);
            ImGui::PopStyleVar();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(300.0f);
        ImGui::InputTextWithHint("##ConsoleSearch", "Search message...", m_SearchText, sizeof(m_SearchText));
        ImGui::Separator();

        const std::vector<LogRecord> liveEntries = LogConsoleStorage::Snapshot();
        if (m_PauseStream)
        {
            if (!m_HasPausedSnapshot)
            {
                m_PausedEntries = liveEntries;
                m_HasPausedSnapshot = true;
            }
        }
        else if (m_HasPausedSnapshot)
        {
            m_PausedEntries.clear();
            m_HasPausedSnapshot = false;
        }

        const std::vector<LogRecord>& entries = m_PauseStream ? m_PausedEntries : liveEntries;

        std::string clipboardText;
        int visibleCount = 0;

        ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        const bool wasAtBottom = (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f);
        int rowIndex = 0;
        for (const LogRecord& entry : entries)
        {
            if (!PassFilter(entry))
            {
                continue;
            }
            ++visibleCount;

            const std::string timestamp = FormatTimestamp(entry);
            const char* channel = entry.GetChannelName();
            const char* level = LogChannelBase::SeverityToString(entry.severity);

            {
                EditorThemeScope rowTheme =
                    EditorWindowTheme::SubduedSectionHeader(m_Context.GetEditorAppearance());
                ImGui::PushID(rowIndex++);
                ImGui::Selectable("##ConsoleRow", false, ImGuiSelectableFlags_SpanAllColumns);
                ImGui::SameLine(0.0f, 6.0f);
                ImGui::TextColored(GetLevelColor(entry.severity),
                                   "[%s] [%s] [%s] %s",
                                   timestamp.c_str(),
                                   channel,
                                   level,
                                   entry.message.c_str());
                ImGui::PopID();
            }

            if (requestCopyVisible)
            {
                clipboardText += "[";
                clipboardText += timestamp;
                clipboardText += "] [";
                clipboardText += channel;
                clipboardText += "] [";
                clipboardText += level;
                clipboardText += "] ";
                clipboardText += entry.message;
                clipboardText += "\n";
            }
        }
        if (m_AutoScroll && wasAtBottom)
        {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::Text("Visible: %d / Total: %d", visibleCount, static_cast<int>(entries.size()));

        if (requestCopyVisible)
        {
            ImGui::SetClipboardText(clipboardText.c_str());
        }
    }

    void ConsoleWindow::DrawCommandTab()
    {
        const CommandConsoleStyle style(m_Context.GetEditorAppearance());

        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
            if (ImGui::Button("Clear##CommandConsole"))
            {
                m_CommandPresenter.ClearOutput();
            }
            ImGui::SameLine();
            ImGui::Checkbox("AutoScroll##CommandConsole", &m_CommandAutoScroll);
            ImGui::SameLine();
            bool showInputEcho = m_CommandPresenter.GetShowInputEcho();
            if (ImGui::Checkbox("Echo input", &showInputEcho))
            {
                m_CommandPresenter.SetShowInputEcho(showInputEcho);
            }
            ImGui::PopStyleVar();
        }

        ImGui::Separator();

        m_CommandPresenter.PrepareCommandTabFrame(m_Context);

        const ImGuiStyle& imguiStyle = ImGui::GetStyle();
        const float separatorChrome = imguiStyle.ItemSpacing.y + 1.0f;
        const float inputChrome = m_CommandPresenter.GetCommandInputRowHeight() + separatorChrome;

        float suggestionsDisplayHeight = 0.0f;
        const float idealSuggestionsHeight = m_CommandPresenter.GetSuggestionsBarHeight();
        if (idealSuggestionsHeight > 0.0f)
        {
            const float regionAvailY = ImGui::GetContentRegionAvail().y;
            const float maxSuggestionsHeight =
                std::max(0.0f, regionAvailY - inputChrome - separatorChrome - 8.0f);
            suggestionsDisplayHeight = std::min(idealSuggestionsHeight, maxSuggestionsHeight);
        }

        float suggestionsChrome = 0.0f;
        if (suggestionsDisplayHeight > 0.0f)
        {
            suggestionsChrome = suggestionsDisplayHeight + separatorChrome;
        }

        const float footerHeight = inputChrome + suggestionsChrome;

        ImGui::BeginChild(
            "CommandConsoleScrollRegion",
            ImVec2(0.0f, -footerHeight),
            false,
            ImGuiWindowFlags_HorizontalScrollbar);
        const bool wasAtBottom = (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f);
        m_CommandPresenter.DrawOutputLines(style);
        if (m_CommandAutoScroll && wasAtBottom)
        {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        if (suggestionsDisplayHeight > 0.0f)
        {
            ImGui::Separator();
            m_CommandPresenter.DrawSuggestionsBar(style, suggestionsDisplayHeight);
        }

        ImGui::Separator();
        ImGui::TextColored(style.GetColor(Command::CommandOutputKind::Path), ">");
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::SetNextItemWidth(-1.0f);
        m_CommandPresenter.DrawInputAndHandleKeys(m_Context, style);
    }

    bool ConsoleWindow::PassFilter(const LogRecord& entry) const
    {
        const char* channelName = entry.GetChannelName();
        if (std::strcmp(channelName, "Core") == 0 && !m_ShowCore)
        {
            return false;
        }
        if (std::strcmp(channelName, "App") == 0 && !m_ShowClient)
        {
            return false;
        }

        if (!PassLevelFilter(entry.severity))
        {
            return false;
        }

        if (m_SearchText[0] == '\0')
        {
            return true;
        }

        return ContainsIgnoreCase(entry.message, m_SearchText);
    }

    bool ConsoleWindow::PassLevelFilter(LogSeverity severity) const
    {
        switch (severity)
        {
            case LogSeverity::Trace: return m_ShowTrace;
            case LogSeverity::Debug: return m_ShowDebug;
            case LogSeverity::Info: return m_ShowInfo;
            case LogSeverity::Warn: return m_ShowWarn;
            case LogSeverity::Error: return m_ShowError;
            case LogSeverity::Fatal: return m_ShowFatal;
            default: return true;
        }
    }

    bool ConsoleWindow::ContainsIgnoreCase(const std::string& text, const char* keyword)
    {
        if (keyword == nullptr || keyword[0] == '\0')
        {
            return true;
        }

        const auto it = std::search(
            text.begin(), text.end(),
            keyword, keyword + std::strlen(keyword),
            [](const char lhs, const char rhs)
            {
                return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
            });
        return it != text.end();
    }

    std::string ConsoleWindow::FormatTimestamp(const LogRecord& entry)
    {
        const std::time_t tt = std::chrono::system_clock::to_time_t(entry.timestamp);
        std::tm localTm = {};
#ifdef _WIN32
        localtime_s(&localTm, &tt);
#else
        localtime_r(&tt, &localTm);
#endif
        char buffer[16] = {};
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", localTm.tm_hour, localTm.tm_min, localTm.tm_sec);
        return std::string(buffer);
    }

    ImVec4 ConsoleWindow::GetLevelColor(LogSeverity severity) const
    {
        const EditorAppearance& appearance = m_Context.GetEditorAppearance();
        const EditorSemanticColors& colors = appearance.GetSemanticColors();
        switch (severity)
        {
            case LogSeverity::Trace:
                return appearance.GetDisplayColor(colors.LogTrace);
            case LogSeverity::Debug:
                return appearance.GetDisplayColor(colors.LogDebug);
            case LogSeverity::Info:
                return appearance.GetDisplayColor(colors.LogInfo);
            case LogSeverity::Warn:
                return appearance.GetDisplayColor(colors.LogWarn);
            case LogSeverity::Error:
                return appearance.GetDisplayColor(colors.LogError);
            case LogSeverity::Fatal:
                return appearance.GetDisplayColor(colors.LogCritical);
            default:
                return appearance.GetDisplayColor(appearance.GetActivePalette().TextPrimary);
        }
    }
}
