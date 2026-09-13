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
        HandleClearOnPlayTransition(isPlaying);

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

    void ConsoleWindow::HandleClearOnPlayTransition(bool isPlaying)
    {
        if (isPlaying && !m_WasPlaying)
        {
            m_PlayStartedAt = std::chrono::system_clock::now();
            m_HasPlayStartedAt = true;
            if (m_ClearOnPlay)
            {
                LogConsoleStorage::Clear();
                m_PausedEntries.clear();
                m_HasPausedSnapshot = false;
                m_ClearedAt = std::chrono::system_clock::now();
            }
        }
        if (!isPlaying)
        {
            m_HasPlayStartedAt = false;
        }
        m_WasPlaying = isPlaying;
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

    void ConsoleWindow::EnsureChannelFilterState()
    {
        LogSystem::ForEachRegisteredChannel(
            [this](LogChannelBase& channel)
            {
                const char* name = channel.GetName();
                if (name == nullptr || name[0] == '\0')
                {
                    return;
                }
                if (m_ChannelEnabled.find(name) == m_ChannelEnabled.end())
                {
                    m_ChannelEnabled.emplace(name, true);
                    m_ChannelOrder.emplace_back(name);
                }
            });
    }

    void ConsoleWindow::DrawOutputTab()
    {
        EnsureChannelFilterState();

        bool requestCopyVisible = false;

        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
            if (ImGui::Button("Clear"))
            {
                LogConsoleStorage::Clear();
                m_PausedEntries.clear();
                m_HasPausedSnapshot = false;
                m_ClearedAt = std::chrono::system_clock::now();
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
            ImGui::SameLine();
            ImGui::Checkbox("Collapse", &m_CollapseDuplicates);
            ImGui::SameLine();
            ImGui::Checkbox("Clear on Play", &m_ClearOnPlay);
            ImGui::PopStyleVar();
        }

        ImGui::Separator();
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
            std::vector<UI::FilterSection> filterSections;
            filterSections.reserve(2);

            UI::FilterSection channelSection;
            channelSection.title = "Channel";
            channelSection.options.reserve(m_ChannelOrder.size());
            for (const std::string& name : m_ChannelOrder)
            {
                auto it = m_ChannelEnabled.find(name);
                if (it == m_ChannelEnabled.end())
                {
                    continue;
                }
                channelSection.options.push_back(UI::FilterOptionRef{it->first.c_str(), &it->second});
            }
            filterSections.push_back(std::move(channelSection));

            filterSections.push_back(
                UI::FilterSection{
                    "Level",
                    {
                        {"Trace", &m_ShowTrace},
                        {"Debug", &m_ShowDebug},
                        {"Info", &m_ShowInfo},
                        {"Warn", &m_ShowWarn},
                        {"Error", &m_ShowError},
                        {"Fatal", &m_ShowFatal},
                    },
                });

            ImGui::SetNextItemWidth(200.0f);
            UI::DrawFilterDropdown("##ConsoleFilterCombo", filterSections);
            ImGui::PopStyleVar();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        const char* timestampItems[] = {"Time: Off", "Time: Clock", "Time: Date"};
        int timestampIndex = static_cast<int>(m_TimestampDisplay);
        if (ImGui::Combo("##ConsoleTimestampMode", &timestampIndex, timestampItems, IM_ARRAYSIZE(timestampItems)))
        {
            m_TimestampDisplay = static_cast<TimestampDisplayMode>(timestampIndex);
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(130.0f);
        const char* timeWindowItems[] = {"Window: Off", "Window: Last N s", "Window: Since Play", "Window: Since Clear"};
        int timeWindowIndex = static_cast<int>(m_TimeWindowMode);
        if (ImGui::Combo("##ConsoleTimeWindow", &timeWindowIndex, timeWindowItems, IM_ARRAYSIZE(timeWindowItems)))
        {
            m_TimeWindowMode = static_cast<TimeWindowMode>(timeWindowIndex);
        }

        if (m_TimeWindowMode == TimeWindowMode::LastSeconds)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            ImGui::DragFloat("##ConsoleLastSeconds", &m_TimeWindowLastSeconds, 0.5f, 0.5f, 3600.0f, "%.1fs");
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##ConsoleSearch", "Search message...", m_SearchText, sizeof(m_SearchText));
        ImGui::Separator();

        const uint64_t liveGeneration = LogConsoleStorage::GetGeneration();
        if (liveGeneration != m_LiveCacheGeneration)
        {
            LogConsoleStorage::CopyInto(m_LiveCache);
            m_LiveCacheGeneration = liveGeneration;
        }

        if (m_PauseStream)
        {
            if (!m_HasPausedSnapshot)
            {
                m_PausedEntries = m_LiveCache;
                m_HasPausedSnapshot = true;
            }
        }
        else if (m_HasPausedSnapshot)
        {
            m_PausedEntries.clear();
            m_HasPausedSnapshot = false;
        }

        const std::vector<LogRecord>& entries = m_PauseStream ? m_PausedEntries : m_LiveCache;
        const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

        std::vector<CollapsedRow> rows;
        BuildCollapsedRows(entries, now, rows);

        std::vector<size_t> drawIndices;
        drawIndices.reserve(rows.size());
        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (rows[i].record != nullptr)
            {
                drawIndices.push_back(i);
            }
        }

        const int visibleCount = static_cast<int>(drawIndices.size());
        std::string clipboardText;

        ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        const bool wasAtBottom = (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f);

        ImGuiListClipper clipper;
        clipper.Begin(visibleCount);
        while (clipper.Step())
        {
            for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex)
            {
                const CollapsedRow& row = rows[drawIndices[static_cast<size_t>(rowIndex)]];
                const LogRecord& entry = *row.record;
                const std::string timestamp = FormatTimestamp(entry);
                const char* channel = entry.GetChannelName();
                const char* level = LogChannelBase::SeverityToString(entry.severity);

                {
                    EditorThemeScope rowTheme =
                        EditorWindowTheme::SubduedSectionHeader(m_Context.GetEditorAppearance());
                    ImGui::PushID(rowIndex);
                    ImGui::Selectable("##ConsoleRow", false, ImGuiSelectableFlags_SpanAllColumns);
                    if (ImGui::IsItemHovered() && entry.source.file != nullptr && entry.source.file[0] != '\0')
                    {
                        ImGui::SetTooltip("%s:%d", entry.source.file, entry.source.line);
                    }
                    ImGui::SameLine(0.0f, 6.0f);
                    if (row.count > 1)
                    {
                        if (timestamp.empty())
                        {
                            ImGui::TextColored(GetLevelColor(entry.severity),
                                               "[%s] [%s] (x%d) %s",
                                               channel,
                                               level,
                                               row.count,
                                               entry.message.c_str());
                        }
                        else
                        {
                            ImGui::TextColored(GetLevelColor(entry.severity),
                                               "[%s] [%s] [%s] (x%d) %s",
                                               timestamp.c_str(),
                                               channel,
                                               level,
                                               row.count,
                                               entry.message.c_str());
                        }
                    }
                    else if (timestamp.empty())
                    {
                        ImGui::TextColored(GetLevelColor(entry.severity),
                                           "[%s] [%s] %s",
                                           channel,
                                           level,
                                           entry.message.c_str());
                    }
                    else
                    {
                        ImGui::TextColored(GetLevelColor(entry.severity),
                                           "[%s] [%s] [%s] %s",
                                           timestamp.c_str(),
                                           channel,
                                           level,
                                           entry.message.c_str());
                    }
                    ImGui::PopID();
                }
            }
        }

        if (requestCopyVisible)
        {
            for (size_t drawIndex : drawIndices)
            {
                const CollapsedRow& row = rows[drawIndex];
                const LogRecord& entry = *row.record;
                const std::string timestamp = FormatTimestamp(entry);
                if (!timestamp.empty())
                {
                    clipboardText += "[";
                    clipboardText += timestamp;
                    clipboardText += "] ";
                }
                clipboardText += "[";
                clipboardText += entry.GetChannelName();
                clipboardText += "] [";
                clipboardText += LogChannelBase::SeverityToString(entry.severity);
                clipboardText += "] ";
                if (row.count > 1)
                {
                    clipboardText += "(x";
                    clipboardText += std::to_string(row.count);
                    clipboardText += ") ";
                }
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

    void ConsoleWindow::BuildCollapsedRows(const std::vector<LogRecord>& entries,
                                           const std::chrono::system_clock::time_point& now,
                                           std::vector<CollapsedRow>& outRows) const
    {
        outRows.clear();
        if (!m_CollapseDuplicates)
        {
            outRows.reserve(entries.size());
            for (const LogRecord& entry : entries)
            {
                if (!PassFilter(entry, now))
                {
                    continue;
                }
                outRows.push_back(CollapsedRow{&entry, 1});
            }
            return;
        }

        for (const LogRecord& entry : entries)
        {
            if (!PassFilter(entry, now))
            {
                continue;
            }

            // Consecutive-only collapse (Unity/UE style): merge with the last visible row only.
            if (!outRows.empty())
            {
                CollapsedRow& lastRow = outRows.back();
                if (lastRow.record != nullptr
                    && lastRow.record->severity == entry.severity
                    && std::strcmp(lastRow.record->GetChannelName(), entry.GetChannelName()) == 0
                    && lastRow.record->message == entry.message)
                {
                    ++lastRow.count;
                    lastRow.record = &entry;
                    continue;
                }
            }

            outRows.push_back(CollapsedRow{&entry, 1});
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

    bool ConsoleWindow::PassFilter(const LogRecord& entry, const std::chrono::system_clock::time_point& now) const
    {
        const char* channelName = entry.GetChannelName();
        if (channelName != nullptr && channelName[0] != '\0')
        {
            const auto it = m_ChannelEnabled.find(channelName);
            if (it != m_ChannelEnabled.end() && !it->second)
            {
                return false;
            }
            // Unknown channel names (not yet in map) remain visible.
        }

        if (!PassLevelFilter(entry.severity))
        {
            return false;
        }

        if (!PassTimeFilter(entry, now))
        {
            return false;
        }

        if (m_SearchText[0] == '\0')
        {
            return true;
        }

        return ContainsIgnoreCase(entry.message, m_SearchText);
    }

    bool ConsoleWindow::PassTimeFilter(const LogRecord& entry,
                                       const std::chrono::system_clock::time_point& now) const
    {
        switch (m_TimeWindowMode)
        {
            case TimeWindowMode::Off:
                return true;
            case TimeWindowMode::LastSeconds:
            {
                const auto age = now - entry.timestamp;
                const auto limit = std::chrono::duration_cast<std::chrono::system_clock::duration>(
                    std::chrono::duration<float>(m_TimeWindowLastSeconds));
                return age <= limit;
            }
            case TimeWindowMode::SincePlay:
                if (!m_HasPlayStartedAt)
                {
                    return true;
                }
                return entry.timestamp >= m_PlayStartedAt;
            case TimeWindowMode::SinceClear:
                return entry.timestamp >= m_ClearedAt;
            default:
                return true;
        }
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

    std::string ConsoleWindow::FormatTimestamp(const LogRecord& entry) const
    {
        if (m_TimestampDisplay == TimestampDisplayMode::Off)
        {
            return {};
        }

        // Default Time mode uses Push-side displayTime (TD-032); avoid per-frame localtime.
        if (m_TimestampDisplay == TimestampDisplayMode::Time)
        {
            return entry.displayTime;
        }

        const std::time_t tt = std::chrono::system_clock::to_time_t(entry.timestamp);
        std::tm localTm = {};
#ifdef _WIN32
        localtime_s(&localTm, &tt);
#else
        localtime_r(&tt, &localTm);
#endif
        char buffer[32] = {};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%04d-%02d-%02d %02d:%02d:%02d",
            localTm.tm_year + 1900,
            localTm.tm_mon + 1,
            localTm.tm_mday,
            localTm.tm_hour,
            localTm.tm_min,
            localTm.tm_sec);
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
